#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define SOURCE_LOC " (" __FILE__ ":" TOSTRING(__LINE__) ")"
// ^ Credit https://www.decompile.com/cpp/faq/file_and_line_error_string.htm

#include "sloth.hpp"
#include "varprops.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <math.h>
#include <stdexcept>
#include <map>
#include <set>
#include <limits>
#include <sstream>
//#include <iostream>

#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

std::string Sloth::GetComponentName(){
  return "Simple Logical Tautology Handler (SLoTH) Model";
}

double Sloth::GetCurrentTime(){
  return current_model_time;
}

double Sloth::GetEndTime(){
  return std::numeric_limits<double>::max();
}

int Sloth::GetGridRank(const int grid){
  throw std::logic_error("GetGridRank Not implemented." SOURCE_LOC);
}

int Sloth::GetGridSize(const int grid){
  throw std::logic_error("GetGridSize Not implemented." SOURCE_LOC);
}

std::string Sloth::GetGridType(const int grid){
  throw std::logic_error("GetGridType Not implemented." SOURCE_LOC);
}

std::vector<std::string> Sloth::GetInputVarNames(){ //v?
  std::set<std::string> ivars;
  for(auto const& iter: this->var_innames)
    ivars.insert(iter.second);
  return std::vector<std::string>(ivars.begin(), ivars.end());
}
std::vector<std::string> Sloth::GetOutputVarNames(){ //v
  std::vector<std::string> ovars;
  for(auto const& iter: this->var_values)
    ovars.push_back(iter.first); 
  return ovars;
}
int Sloth::GetInputItemCount(){ //v
  return this->GetInputVarNames().size();
}
int Sloth::GetOutputItemCount(){ //v
  return this->var_values.size();
}

double Sloth::GetStartTime(){ //v
  return 0.0;
}

double Sloth::GetTimeStep(){ //v
  return std::numeric_limits<double>::lowest();
}

std::string Sloth::GetTimeUnits(){ //v
  return "s";
}

void Sloth::GetValue(std::string name, void* dest){ //v
  name = this->ResolveInNameAlias(name);

  void *src;
  src = this->GetValuePtr(name);

  int nbytes = this->GetVarNbytes(name);
  std::memcpy (dest, src, nbytes);
}

void Sloth::GetValueAtIndices(std::string name, void* dest, int* inds, int count){ //v
  name = this->ResolveInNameAlias(name);

  if (count < 1)
    throw std::runtime_error(std::string("Illegal count ") + std::to_string(count) + std::string(" provided to GetValueAtIndices(name, dest, inds, count)" SOURCE_LOC));

  void *src;
  src = this->GetValuePtr(name);

  char* srcbyte = (char*)src;
  char* destbyte = (char*)dest;

  int itemsize = this->GetVarItemsize(name);

  for (size_t i = 0; i < count; ++i) {
    std::memcpy(
      destbyte + (itemsize * i), 
      srcbyte + (itemsize * inds[i]), 
      itemsize 
      );
  }
  return;
}

void* Sloth::GetValuePtr(std::string name){ //v
  if (name == "serialization_state") {
    if (this->m_serialized_length == 0) {
      throw std::runtime_error("Cannot get the current serialization state before creating one.");
    }
    return (void *)this->m_serialized.data();
  } else if (name == "serialization_size") {
    return (void *)(&this->m_serialized_length);
  }
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_values.find(name);
  if(iter != this->var_values.end()){
    return iter->second.get();
  }
  throw std::runtime_error("GetValuePtr called for unknown variable: " + name + "" + SOURCE_LOC );
}

int Sloth::GetVarItemsize(std::string name){ //v
  name = this->ResolveInNameAlias(name);

  std::map<std::string,int>::const_iterator iter = this->type_sizes.find(this->GetVarType(name));
  if(iter != this->type_sizes.end()){
    return iter->second;
  }
  throw std::runtime_error("Item \""+name+"\" has illegal type \""+(this->GetVarType(name))+"\"! " SOURCE_LOC);
}

std::string Sloth::GetVarLocation(std::string name){ //v
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_locations.find(name);
  if(iter != this->var_locations.end()){
    return iter->second;
  }
  throw std::runtime_error("GetVarLocation called for non-existent variable: "+name+" " SOURCE_LOC);
}

int Sloth::GetVarNbytes(std::string name){ //v
  if (name == "serialization_create" || name == "serialization_size") {
    return sizeof(uint64_t);
  } else if (name == "serialization_state") {
    return this->m_serialized_length;
  } else if (name == "serialization_free") {
    return sizeof(int);
  } else if (name == "reset_size") {
    return sizeof(double);
  }
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  if(var_nbytes.count(name) <= 0){
    throw std::runtime_error("GetVarNbytes called for non-existent variable "+name+" " SOURCE_LOC);
  }
  int nbytes = var_nbytes[name];
  if(nbytes <= 0){
    return ComputeVarNbytes(name);
  }
  return var_nbytes[name];
}

std::string Sloth::GetVarType(std::string name){ //v
  if (name == "serialization_create" || name == "serialization_size") {
    return "uint64_t";
  } else if (name == "serialization_state") {
    return "char";
  } else if (name == "serialization_free") {
    return "int";
  } else if (name == "reset_size") {
    return "double";
  }
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_types.find(name);
  if(iter != this->var_types.end()){
    return iter->second;
  }
  throw std::runtime_error("GetVarType called for non-existent variable: "+name+" " SOURCE_LOC );
}

std::string Sloth::GetVarUnits(std::string name){ //v
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_units.find(name);
  if(iter != this->var_units.end()){
    const std::string& u = iter->second;
    // Normalize common "no units" spellings to UDUNITS' dimensionless "1"
    if (u.empty() || u == "none" || u == "-")
      return "1";
    return u;
  }
  throw std::runtime_error("GetVarUnits called for non-existent variable: "+name+" " SOURCE_LOC );
}

void Sloth::Initialize(std::string file){ //v
  this->current_model_time = this->GetStartTime();
}

void Sloth::SetValueAtIndices(std::string name, int* inds, int count, void* src){ //v
  if (count < 1)
    throw std::runtime_error(std::string("Illegal count ") + std::to_string(count) + std::string(" provided to SetValueAtIndices(name, dest, inds, count)" SOURCE_LOC));

  // If this is actually destined for an input alias, punt!...
  auto aliases = this->ResolveInNameAliases(name);
  if(!aliases.empty()){
    for(std::string inname : aliases){
      //TODO: Might be worth wrapping this in try/catch and wrapping any exceptions with a note about the actual name tried...
      this->SetValueAtIndices(inname, inds, count, src);
    }
    return;
  }
  // Otherwise...

  void* ptr;

  // If this somehow gets called first, we will need space as if we are setting by value. This *should* never happen.
  name = this->ProcessNameMeta(name);

  void *dest;
  dest = this->GetValuePtr(name);

  
  char* srcbyte = (char*)src;
  char* destbyte = (char*)dest;

  int itemsize = this->GetVarItemsize(name);

  for (size_t i = 0; i < count; ++i) {
    std::memcpy(
      destbyte + (itemsize * inds[i]), 
      srcbyte + (itemsize * i), 
      itemsize 
      );
  }
  return;  
}

void Sloth::SetValue(std::string name, void* src){ //v
  if (name == "serialization_create") {
    this->new_serialized();
    return;
  } else if (name == "serialization_state") {
    this->load_serialized(static_cast<const char*>(src));
    return;
  } else if (name == "serialization_free") {
    this->free_serialized();
    return;
  } else if (name == "reset_time") {
    this->current_model_time = this->GetStartTime();
    return;
  } else if (name == "serialization_size") {
    throw std::runtime_error("SetValue is not a valid option for \"serialization_size\".");
  }
  // If this is actually destined for an input alias, punt!...
  auto aliases = this->ResolveInNameAliases(name);
  if(!aliases.empty()){
    for(std::string inname : aliases){
      //TODO: Might be worth wrapping this in try/catch and wrapping any exceptions with a note about the actual name tried...
      this->SetValue(inname, src);
    }
    return;
  }
  // Otherwise...

  name = this->ProcessNameMeta(name);

  void *dest = this->GetValuePtr(name);
  int nbytes = this->GetVarNbytes(name);
  std::memcpy (dest, src, nbytes);
}

void Sloth::Update(){ //v
  this->UpdateUntil(this->current_model_time + this->GetTimeStep());
}

void Sloth::UpdateUntil(double future_time){ //v
  if (this->current_model_time != future_time)
    this->current_model_time = future_time;
}

void Sloth::Finalize(){ //v
  //TODO: Consider resetting state here in case the object is reused?
  return;
}

int Sloth::GetGridEdgeCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetGridFaceCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridEdgeNodes(const int grid, int* edge_nodes){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridFaceEdges(const int grid, int* face_edges){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridFaceNodes(const int grid, int* face_nodes){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetGridNodeCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridNodesPerFace(const int grid, int* nodes_per_face){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridOrigin(const int grid, double* origin){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridShape(const int grid, int* shape){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridSpacing(const int grid, double* spacing){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridX(const int grid, double* x){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridY(const int grid, double* y){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridZ(const int grid, double* z){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetVarGrid(std::string name){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}

int Sloth::ComputeVarNbytes(std::string name){ //v
  int item_size = this->GetVarItemsize(name);
  // Note that if this name is not already in state that the above call will throw...
  // so we'll omit checking from here on.

  int item_count;
  std::map<std::string,int>::const_iterator iter = this->var_counts.find(name);
  if(iter != this->var_counts.end()){
    item_count = iter->second;
  }

  return item_size * item_count;
}

std::string Sloth::ProcessNameMeta(std::string name){ //v
  // Early-out: if the name passed is already known, it can be assumed that it has no metadata--return it.
  if(this->var_counts.count(name) > 0){
    return name;
  }
  // Early-out: if the name passed in is an input alias, will not process metadata, return it.
  if(!this->ResolveInNameAliases(name).empty()){
    return name;
  }

  // parse name string for metadata
  int count = 1;
  std::string type = "double";
  std::string units = "1";
  std::string location = "node";
  std::string inname = "";

  // parse name string for metadata
  std::string raw_name, tempstr;
  size_t lpos, rpos, temppos, rppos;
  if((lpos = name.find("(")) != std::string::npos){

    if((rpos = name.find(")")) == std::string::npos){
      throw std::runtime_error("Missing closing paren in variable definition '" + name + "' " + SOURCE_LOC);
    }
    rppos = rpos;
    raw_name = name.substr(0,lpos);

    // find a comma, otherwise rpos remains the right paren location
    if((temppos = name.find(",",lpos+1)) != std::string::npos){
      rpos = temppos;
    }
    tempstr = name.substr(lpos+1,rpos-lpos-1);
    //TODO: support whitespace trimming https://www.techiedelight.com/trim-string-cpp-remove-leading-trailing-spaces/
    if(tempstr.length()>0){
      count = std::stoi(tempstr);
    }
    lpos = rpos;
    rpos = rppos;

    if((temppos = name.find(",",lpos+1)) != std::string::npos){
      rpos = temppos;
    }
    tempstr = name.substr(lpos+1,rpos-lpos-1);
    if(tempstr.length()>0){
      type = tempstr;
      if(type_sizes.count(type) <= 0){
        throw std::runtime_error("Illegal type '" + type + "' specified for variable '" + raw_name + "' " SOURCE_LOC);
      }
    }
    lpos = rpos;
    rpos = rppos;

    if((temppos = name.find(",",lpos+1)) != std::string::npos){
      rpos = temppos;
    }
    tempstr = name.substr(lpos+1,rpos-lpos-1);
    if(tempstr.length()>0){
      units = tempstr;
      // Normalize common "no units" spellings to UDUNITS' dimensionless "1"
      if (units.empty() || units == "none" || units == "-")
        units = "1";
    }

    lpos = rpos;
    rpos = rppos;

    if((temppos = name.find(",",lpos+1)) != std::string::npos){
      rpos = temppos;
    }
    tempstr = name.substr(lpos+1,rpos-lpos-1);
    if(tempstr.length()>0){
      location = tempstr;
    }
    lpos = rpos;
    rpos = rppos;

    if((temppos = name.find(",",lpos+1)) != std::string::npos){
      rpos = temppos;
    }
    tempstr = name.substr(lpos+1,rpos-lpos-1);
    if(tempstr.length()>0){
      inname = tempstr;
    }

    // Validate non-collision for inname
    if(inname == raw_name){
      throw std::runtime_error("Aliasing an input variable (\"" + inname + "\") to its own name is not allowed!");
    }
    if(this->var_values.count(inname) > 0){
      throw std::runtime_error("Attempt to set input alias \"" + inname + "\" for variable \"" + raw_name + "\" conflicts with existing output variable of the same name, which is not allowed!");
    }

  } else {
    raw_name = name;
  }

  // If this is a new name, make sure it does not collide with a previously defined input alias
  // (Checking first if it is new is an optimization)
  if(this->var_counts.count(raw_name) <= 0){
    if(!this->ResolveInNameAliases(raw_name).empty()){
      throw std::runtime_error("Attempt to define a new variable \"" + raw_name + "\" which conflicts with a previously defined input alias of the same name, which is not allowed!");
    }
  }

  var_units[raw_name] = units;
  var_counts[raw_name] = count;
  var_types[raw_name] = type;
  var_locations[raw_name] = location;
  if(inname != ""){
	  var_innames[raw_name] = inname;
  }
  //std::cerr<<"ProcessNameMeta processed "<<raw_name<<"("<<var_counts[raw_name]<<","<<type<<","<<units<<","<<location<<","<<inname<<")"<<std::endl;
  this->EnsureAllocatedForByValue(raw_name);

  return raw_name;
}

std::string Sloth::ResolveInNameAlias(std::string name){
  //TODO: Make a shortcut reverse map for this case?
  if (is_serialization_name(name)) {
      return name;
  }
  std::string retval = name;
  for (auto iter = this->var_innames.begin(); iter != this->var_innames.end(); ++iter)
    if (iter->second == name){
      retval = iter->first;
      break;
    }
  return retval;
}

std::vector<std::string> Sloth::ResolveInNameAliases(std::string name){
  std::vector<std::string> retval;
  if (is_serialization_name(name)) {
    retval.push_back(name);
  } else {
    for (auto iter = this->var_innames.begin(); iter != this->var_innames.end(); ++iter)
      if (iter->second == name)
        retval.push_back(iter->first);
  }
  return retval;
}

void Sloth::EnsureAllocatedForByValue(std::string name){
  int nbytes = this->var_nbytes[name]; // Note that this will set a default of zero!
  if(nbytes == 0){
    // New varaible! We are setting by value, so set up some memory we will own...
    //TODO: May need some try/catch/finally to ensure consistency between var_nbytes and var_values?
    nbytes = this->var_nbytes[name] = this->ComputeVarNbytes(name);
    this->var_values.emplace(name, std::shared_ptr<void>(std::malloc(nbytes), std::free));
  }
}

template<class Archive>
void Sloth::serialize(Archive &ar, const unsigned int version) {
  ar & this->current_model_time;

  // store the current number of items
  int var_size = this->var_values.size();
  ar & var_size;

  VariableProps props;
  // if saving, just loop through current values and store them
  if (Archive::is_saving::value) {
    for (auto const &value : this->var_values) {
      props.name = value.first;
      props.value = value.second;
      props.nbytes = this->var_nbytes[value.first];
      props.units = this->var_units[value.first];
      props.type = this->var_types[value.first];
      props.location = this->var_locations[value.first];
      props.count = this->var_counts[value.first];
      auto inname_it = this->var_innames.find(value.first);
      if (inname_it == this->var_innames.end()) {
        props.inname = "";
      } else {
        props.inname = *inname_it;
      }
      ar & props;
    }
  }
  // if loading, clear the current data and load as many values as were said to be stored
  else {
    this->var_values.clear();
    this->var_units.clear();
    this->var_types.clear();
    this->var_locations.clear();
    this->var_counts.clear();
    this->var_innames.clear();
    this->var_nbytes.clear();
    while (--var_size >= 0) {
      ar & props;
      this->var_values[props.name] = props.value;
      this->var_units[props.name] = props.units;
      this->var_types[props.name] = props.type;
      this->var_locations[props.name] = props.location;
      this->var_counts[props.name] = props.count;
      this->var_nbytes[props.name] = props.nbytes;
      if (!props.inname.empty()) {
        this->var_innames[props.name] = props.inname;
      }
    }
  }
}

void Sloth::new_serialized() {
  this->m_serialized.clear();
  boost::archive::binary_oarchive archive(this->m_serialized);
  try {
    archive << (*this);
    this->m_serialized_length = this->m_serialized.size();
  } catch (const std::exception &e) {
    this->m_serialized_length = 0;
    throw;
  }
}

void Sloth::load_serialized(const char* data) {
  std::stringstream stream(data);
  boost::archive::binary_iarchive archive(stream);
  try {
    archive >> (*this);
  } catch (const std::exception &e) {
    // possible logging when implemented
    throw;
  }
  this->free_serialized();
}

void Sloth::free_serialized() {
  this->m_serialized.clear();
  this->m_serialized.shrink_to_fit();
  this->m_serialized_length = 0;
}

bool Sloth::is_serialization_name(const std::string &name) {
  return name == "serialization_state"
    || name == "serialization_size"
    || name == "serialization_create"
    || name == "serialization_free"
    || name == "reset_size";
}
