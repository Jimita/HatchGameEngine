#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Graphics.h>

class IModel {
public:
    vector<vector<IFace>>     Faces;
    vector<vector<IVertex>>   Vertices;
    vector<vector<IVertex>>   Normals;
    vector<vector<ITexCoord>> UVs;
    vector<vector<IColor>>    Colors;
    vector<IMaterial>         Materials;

    int                       MeshCount;
    vector<int>               FaceCount;
};
#endif

#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ModelFormats/COLLADA.h>
#include <Engine/ResourceTypes/ModelFormats/COLLADAReader.h>

#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>

PUBLIC IModel::IModel() {

}

PUBLIC IModel::IModel(const char* filename) {
    IModel *imodel = COLLADAReader::Convert(filename);
    Colors = imodel->Colors;
    Faces = imodel->Faces;
    Vertices = imodel->Vertices;
    Normals = imodel->Normals;
    UVs = imodel->UVs;
    Materials = imodel->Materials;
    MeshCount = imodel->MeshCount;
    FaceCount = imodel->FaceCount;
    delete imodel;
}

PUBLIC bool IModel::HasColorsInMesh(int meshIndex) {
    return (Colors[meshIndex].size() > 0);
}

PUBLIC bool IModel::HasTexturesInMesh(int meshIndex) {
    return (UVs[meshIndex].size() > 0);
}

PUBLIC bool IModel::HasNormalsInMesh(int meshIndex) {
    return (Normals[meshIndex].size() > 0);
}

PUBLIC bool IModel::HasTextureInMesh(int meshIndex) {
    //Log::Print(Log::LOG_INFO, "IModel::HasTextureInMesh %d", Materials.size());
    if (Materials.size() == 0)
        return false;
    //Log::Print(Log::LOG_INFO, "IModel::HasTextureInMesh: %p", Materials[meshIndex].Tex);
    return (Materials[meshIndex].Tex != nullptr);
}

PUBLIC void IModel::Dispose() {
    // ???
}
