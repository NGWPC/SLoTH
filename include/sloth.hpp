#ifndef SLOTH_H
#define SLOTH_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include "bmi.hxx"

#define BMI_TYPE_NAME_DOUBLE "double"
#define BMI_TYPE_NAME_FLOAT "float"
#define BMI_TYPE_NAME_INT "int"
#define BMI_TYPE_NAME_SHORT "short"
#define BMI_TYPE_NAME_LONG "long"

class Sloth : public bmi::Bmi {
    public:
        /**
        * Create a new model data struct instance, allocating memory for the struct itself but not any pointers within it.
        *
        * The ``time_step_size`` member is set to a defined default.  All other members are set to ``0`` or ``NULL`` (for
        * pointers).
        *
        * @return Pointer to the newly created @ref test_bmi_c_model struct instance in memory.
        */
        Sloth(){};

        void Initialize(std::string config_file) override;
        void Update() override;
        void UpdateUntil(double time) override;
        void Finalize() override;

        // Model information functions.
        std::string GetComponentName() override;
        int GetInputItemCount() override;
        int GetOutputItemCount() override;
        std::vector<std::string> GetInputVarNames() override;
        std::vector<std::string> GetOutputVarNames() override;

        // Variable information functions
        int GetVarGrid(std::string name) override;
        std::string GetVarType(std::string name) override;
        std::string GetVarUnits(std::string name) override;
        int GetVarItemsize(std::string name) override;
        int GetVarNbytes(std::string name) override;
        std::string GetVarLocation(std::string name) override;

        double GetCurrentTime() override;
        double GetStartTime() override;
        double GetEndTime() override;
        std::string GetTimeUnits() override;
        double GetTimeStep() override;

        // Variable getters
        void GetValue(std::string name, void *dest) override;
        void *GetValuePtr(std::string name) override;
        void GetValueAtIndices(std::string name, void *dest, int *inds, int count) override;

        // Variable setters
        void SetValue(std::string name, void *src) override;
        void SetValueAtIndices(std::string name, int *inds, int count, void *src) override;

        // Grid information functions
        int GetGridRank(const int grid) override;
        int GetGridSize(const int grid) override;
        std::string GetGridType(const int grid) override;

        void GetGridShape(const int grid, int *shape) override;
        void GetGridSpacing(const int grid, double *spacing) override;
        void GetGridOrigin(const int grid, double *origin) override;

        void GetGridX(int grid, double *x) override;
        void GetGridY(const int grid, double *y) override;
        void GetGridZ(const int grid, double *z) override;

        int GetGridNodeCount(const int grid) override;
        int GetGridEdgeCount(const int grid) override;
        int GetGridFaceCount(const int grid) override;

        void GetGridEdgeNodes(const int grid, int *edge_nodes) override;
        void GetGridFaceEdges(const int grid, int *face_edges) override;
        void GetGridFaceNodes(const int grid, int *face_nodes) override;
        void GetGridNodesPerFace(const int grid, int *nodes_per_face) override;

    private:
        double current_model_time = 0.0;

        std::map<std::string, std::shared_ptr<void>> var_values;
        std::map<std::string, std::string> var_units;
        std::map<std::string, std::string> var_types;
        std::map<std::string, std::string> var_locations;
        std::map<std::string, int> var_counts;
        std::map<std::string, std::string> var_innames;
        // Number of bytes last stored, or (TODO?) <=0 if passed in by pointer (i.e. we don't own the memory).
        std::map<std::string, int> var_nbytes; 

        std::map<std::string,int> type_sizes = {
            {BMI_TYPE_NAME_DOUBLE, sizeof(double)},
            {BMI_TYPE_NAME_FLOAT, sizeof(float)},
            {BMI_TYPE_NAME_INT, sizeof(int)},
            {BMI_TYPE_NAME_SHORT, sizeof(short)},
            {BMI_TYPE_NAME_LONG, sizeof(long)}
        };

        int ComputeVarNbytes(std::string name);

        /**
         * Return any output name for an input alias, or if the name is not an alias, return it.
         * This is sufficient for most "Get" methods because the value across multiple potential
         * outputs should be the same.
         */ 
        std::string ResolveInNameAlias(std::string name);

        std::vector<std::string> ResolveInNameAliases(std::string name); // only used for Setting

        /**
         * @brief Parses any metadata appended to a variable name used in a `Set*` operation and updates the model's state accordingly.
         * 
         * Uses the following default metadata values if a name is encountered for the first time and any are not specified:
         * 
         * * type = `double`
         * * units = `1` (dimensionless unit value)
         * * location = "node"
         * 
         * If metadata is found on a variable that has been defined as an input variable, this method will throw.
         * 
         * @param nameMaybeWithMeta A variable name passed to some `Set...` operation which may or may not have metadata about the variable encoded in parentheses.
         * @return std::string The "raw" name with the metadata information (if any) removed.
         */
        std::string ProcessNameMeta(std::string nameMaybeWithMeta);
        void EnsureAllocatedForByValue(std::string name);

};

extern "C"
{
    /**
    * Construct this BMI instance as a normal C++ object, to be returned to the framework.
    *
    * @return A pointer to the newly allocated instance.
    */
	Sloth *bmi_model_create()
	{
		return new Sloth();
	}

    /**
     * @brief Destroy/free an instance created with @see bmi_model_create
     * 
     * @param ptr 
     */
	void bmi_model_destroy(Sloth *ptr)
	{
		delete ptr;
	}
}

#endif //SLOTH_H
