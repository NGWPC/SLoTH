#include <string>
#include <memory>

#include <boost/serialization/access.hpp>
#include <boost/serialization/array.hpp>

struct VariableProps {
public:
    VariableProps() = default;
    ~VariableProps() = default;

    std::string name;
    std::string units;
    std::string type;
    std::string location;
    std::string inname;
    int count;
    int nbytes;
    std::shared_ptr<void> value;

private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & this->name;
        ar & this->units;
        ar & this->type;
        ar & this->location;
        ar & this->inname;
        ar & this->count;
        ar & this->nbytes;
        if (Archive::is_loading::value) {
            // if loading, allocate memory for the load size
            this->value = std::shared_ptr<void>(std::malloc(this->nbytes), std::free);
        }
        ar & boost::serialization::make_array(
            static_cast<char *>(this->value.get()), this->nbytes
        );
    }

};
