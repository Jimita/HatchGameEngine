#include <Engine/ResourceTypes/ModelFormats/COLLADA.h>
#include <Engine/ResourceTypes/ModelFormats/COLLADAReader.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/IO/Stream.h>

#if INTERFACE
class COLLADAReader {
public:

};
#endif

static void TokenToString(Token tok, char** string)
{
    *string = (char* )calloc(1, tok.Length + 1);
    memcpy(*string, tok.Start, tok.Length);
}

// Caveat: This modifies the source string.
static void ParseFloatArray(vector<float> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size)
    {
        if (*source == '\0' || *source == ' ' || *source == '\n')
        {
            *source = '\0';

            if ((source - start) > 0)
            {
                float fnum = atof(start);
                dest.push_back(fnum);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

// So does this function.
static void ParseIntegerArray(vector<int> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size)
    {
        if (*source == '\0' || *source == ' ' || *source == '\n')
        {
            *source = '\0';

            if ((source - start) > 0)
            {
                int num = atoi(start);
                dest.push_back(num);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

static void FloatStringToArray(XMLNode* contents, float* array, int size, float defaultval)
{
    int i;
    vector<float> vec;

    vec.resize(size);
    for (i = 0; i < size; i++)
        vec[i] = defaultval;

    if (contents->name.Length) {
        char* list = (char* )calloc(1, contents->name.Length + 1);
        memcpy(list, contents->name.Start, contents->name.Length);
        ParseFloatArray(vec, list);
        free(list);
    }

    for (i = 0; i < size; i++)
        array[i] = vec[i];
}

//
// ASSET PARSING
//

PRIVATE STATIC void COLLADAReader::ParseAsset(ColladaModel* model, XMLNode* asset) {
    //Log::Print(Log::LOG_INFO, "Parsing asset...");
    for (size_t i = 0; i < asset->children.size(); i++) {
        XMLNode* node = asset->children[i];
        if (XMLParser::MatchToken(node->name, "up_axis")) {
            XMLNode* axis = node->children[0];

            //Log::Print(Log::LOG_INFO, "Found axis");

            if (XMLParser::MatchToken(axis->name, "X_UP"))
                model->Axis = DAE_X_UP;
            else if (XMLParser::MatchToken(axis->name, "Y_UP"))
                model->Axis = DAE_Y_UP;
            else if (XMLParser::MatchToken(axis->name, "Z_UP"))
                model->Axis = DAE_Z_UP;
        }
    }
}

//
// LIBRARY PARSING
//

static ColladaMeshSource* FindMeshSource(vector<ColladaMeshSource*> list, char* name)
{
    //Log::Print(Log::LOG_INFO, "FindMeshSource: %d", list.size());
    for (int i = 0; i < list.size(); i++) {
        //Log::Print(Log::LOG_INFO, "FindMeshSource: %s %s", list[i]->Id, name);
        if (!strcmp(list[i]->Id, name))
            return list[i];
    }

    return nullptr;
}

static ColladaMeshFloatArray* FindAccessorSource(vector<ColladaMeshFloatArray*> arrays, char* name)
{
    //Log::Print(Log::LOG_INFO, "FindAccessorSource: %d", arrays.size());
    for (int i = 0; i < arrays.size(); i++) {
        //Log::Print(Log::LOG_INFO, "FindAccessorSource: %s %s", arrays[i]->Id, name);
        if (!strcmp(arrays[i]->Id, name))
            return arrays[i];
    }

    return nullptr;
}

PRIVATE STATIC void COLLADAReader::ParseMeshSource(ColladaMesh* daemesh, XMLNode* node) {
    ColladaMeshSource* source = new ColladaMeshSource;

    TokenToString(node->attributes.Get("id"), &source->Id);

    // Find arrays first
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "float_array")) {
            ColladaMeshFloatArray* floatArray = new ColladaMeshFloatArray;

            TokenToString(child->attributes.Get("id"), &floatArray->Id);
            floatArray->Count = XMLParser::TokenToNumber(child->attributes.Get("count"));

            XMLNode* contents = child->children[0];
            if (contents->name.Length) {
                char* list = (char* )calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseFloatArray(floatArray->Contents, list);
                free(list);
            }

            source->FloatArrays.push_back(floatArray);
        }
    }

    // Then find accessors
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "technique_common")) {
            XMLNode* accessorNode = child->children[0];
            ColladaMeshAccessor* accessor = new ColladaMeshAccessor;

            char* accessorSource;
            TokenToString(accessorNode->attributes.Get("source"), &accessorSource);
            accessor->Source = FindAccessorSource(source->FloatArrays, (accessorSource + 1));
            accessor->Count = XMLParser::TokenToNumber(accessorNode->attributes.Get("count"));
            accessor->Stride = XMLParser::TokenToNumber(accessorNode->attributes.Get("stride"));

            /*
            //Log::Print(Log::LOG_INFO,
            "source: %s %p\ncount: %d\nstride: %d",
            accessorSource, accessor->Source, accessor->Count, accessor->Stride);
            */

            // Parameters are irrelevant
            free(accessorSource);

            source->Accessors.push_back(accessor);
        }
    }

    daemesh->SourceList.push_back(source);
}

PRIVATE STATIC void COLLADAReader::ParseMeshVertices(ColladaMesh* daemesh, XMLNode* node) {
    TokenToString(node->attributes.Get("id"), &daemesh->Vertices.Id);

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);
            input->Source = FindMeshSource(daemesh->SourceList, (inputSource + 1));
            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));
            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            daemesh->Vertices.Inputs.push_back(input);
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseMeshTriangles(ColladaMesh* daemesh, XMLNode* node) {
    TokenToString(node->attributes.Get("id"), &daemesh->Triangles.Id);
    daemesh->Triangles.Count = XMLParser::TokenToNumber(node->attributes.Get("count"));

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (XMLParser::MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            if (!input->Semantic)
            {
                delete input;
                continue;
            }

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);

            if (!strcmp(input->Semantic, "VERTEX"))
            {
                if (!strcmp(daemesh->Vertices.Id, (inputSource + 1)))
                {
                    if (daemesh->Vertices.Inputs.size() > 1)
                    {
                        for (int j = 0; j < daemesh->Vertices.Inputs.size(); j++)
                            input->Children.push_back(daemesh->Vertices.Inputs[j]);
                        input->Source = nullptr;
                    }
                    else
                    {
                        //Log::Print(Log::LOG_INFO, "Copying mesh triangles inputs from %s", inputSource);
                        daemesh->Triangles.Inputs.push_back(daemesh->Vertices.Inputs[0]);
                        delete input;
                        continue;
                    }
                }
            }
            else
            {
                //Log::Print(Log::LOG_INFO, "Adding mesh triangles input %s", inputSource);
                input->Source = FindMeshSource(daemesh->SourceList, (inputSource + 1));
            }

            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));

            daemesh->Triangles.Inputs.push_back(input);
        } else if (XMLParser::MatchToken(child->name, "p")) {
            XMLNode* contents = child->children[0];
            if (contents->name.Length)
            {
                char* list = (char* )calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseIntegerArray(daemesh->Triangles.Primitives, list);
                free(list);

                //Log::Print(Log::LOG_INFO, "daemesh->Triangles.Primitives.size(): %d", daemesh->Triangles.Primitives.size());
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseGeometry(ColladaModel* model, XMLNode* geometry) {
    ColladaGeometry* daegeometry = new ColladaGeometry;

    TokenToString(geometry->attributes.Get("id"), &daegeometry->Id);
    TokenToString(geometry->attributes.Get("name"), &daegeometry->Name);
    //Log::Print(Log::LOG_INFO, "ParseGeometry: %s %s", daegeometry->Id, daegeometry->Name);

    for (size_t i = 0; i < geometry->children.size(); i++) {
        XMLNode* node = geometry->children[i];

        if (XMLParser::MatchToken(node->name, "mesh")) {
            //Log::Print(Log::LOG_INFO, "Parsing mesh...");
            ParseMesh(model, node);
            daegeometry->Mesh = model->Meshes.back();
        }
    }

    model->Geometries.push_back(daegeometry);
}

PRIVATE STATIC void COLLADAReader::ParseMesh(ColladaModel* model, XMLNode* mesh) {
    ColladaMesh* daemesh = new ColladaMesh;

    daemesh->Material = nullptr;

    for (size_t i = 0; i < mesh->children.size(); i++) {
        XMLNode* node = mesh->children[i];

        if (XMLParser::MatchToken(node->name, "source")) {
            //Log::Print(Log::LOG_INFO, "Found source!");
            ParseMeshSource(daemesh, node);
        } else if (XMLParser::MatchToken(node->name, "vertices")) {
            //Log::Print(Log::LOG_INFO, "Found vertices!");
            ParseMeshVertices(daemesh, node);
        } else if (XMLParser::MatchToken(node->name, "triangles")) {
            //Log::Print(Log::LOG_INFO, "Found triangles!");
            ParseMeshTriangles(daemesh, node);
        }
    }

    model->Meshes.push_back(daemesh);
}

PRIVATE STATIC void COLLADAReader::ParseImage(ColladaModel* model, XMLNode* parent, XMLNode* effect) {
    ColladaImage* daeimage = new ColladaImage;

    TokenToString(parent->attributes.Get("id"), &daeimage->Id);
    //Log::Print(Log::LOG_INFO, "Image name: %s", daeimage->Id);

    XMLNode* path = effect->children[0];
    if (path->name.Length) {
        char* pathstring = (char* )calloc(1, path->name.Length + 1);
        memcpy(pathstring, path->name.Start, path->name.Length);
        daeimage->Path = pathstring;
        //Log::Print(Log::LOG_INFO, "Image path: %s", daeimage->Path);
    }

    if (!daeimage->Path)
        return;
    else {
        char* str = daeimage->Path;
        char* filename;
        int i = (int)strlen(str);
        while (i-- >= 0)
        {
            if (str[i] == '/' || str[i] == '\\')
            {
                filename = &str[i+1];
                break;
            }
        }

        if (!filename || filename[0] == '\0')
            return;

        char* texpath = "Models\\Textures\\";
        free(daeimage->Path);
        daeimage->Path = (char*)calloc(strlen(texpath) + strlen(filename) + 1, 1);
        strcpy(daeimage->Path, texpath);
        strcat(daeimage->Path, filename);

        //Log::Print(Log::LOG_INFO, "New filename: %s", daeimage->Path);
    }

    model->Images.push_back(daeimage);
}

PRIVATE STATIC void COLLADAReader::ParseSurface(ColladaModel* model, XMLNode* parent, XMLNode* surface) {
    ColladaSurface* daesurface = new ColladaSurface;

    TokenToString(parent->attributes.Get("sid"), &daesurface->Id);
    TokenToString(surface->attributes.Get("type"), &daesurface->Type);

    //Log::Print(Log::LOG_INFO, "Surface name: %s", daesurface->Id);
    //Log::Print(Log::LOG_INFO, "Surface type: %s", daesurface->Type);

    daesurface->Image = nullptr;

    for (size_t i = 0; i < surface->children.size(); i++) {
        XMLNode* node = surface->children[i];
        if (XMLParser::MatchToken(node->name, "init_from")) {
            XMLNode* child = node->children[0];
            if (child->name.Length) {
                char* imagestring = (char* )calloc(1, child->name.Length + 1);
                memcpy(imagestring, child->name.Start, child->name.Length);

                for (size_t i = 0; i < model->Images.size(); i++) {
                    ColladaImage* image = model->Images[i];
                    //Log::Print(Log::LOG_INFO, "image %s", image->Id);
                    if (!strcmp(imagestring, image->Id)) {
                        daesurface->Image = image;
                        break;
                    }
                }

                free(imagestring);
            }
        }
    }

    model->Surfaces.push_back(daesurface);
}

PRIVATE STATIC void COLLADAReader::ParseSampler(ColladaModel* model, XMLNode* parent, XMLNode* sampler) {
    ColladaSampler* daesampler = new ColladaSampler;

    TokenToString(parent->attributes.Get("sid"), &daesampler->Id);
    //Log::Print(Log::LOG_INFO, "Sampler name: %s", daesampler->Id);

    daesampler->Surface = nullptr;

    for (size_t i = 0; i < sampler->children.size(); i++) {
        XMLNode* node = sampler->children[i];
        // Sampler comes after surface so this is fine
        if (XMLParser::MatchToken(node->name, "source")) {
            XMLNode* child = node->children[0];

            if (!child->name.Length)
                continue;

            char* source = (char* )calloc(1, child->name.Length + 1);
            memcpy(source, child->name.Start, child->name.Length);
            //Log::Print(Log::LOG_INFO, "ParseSampler: source %s", source);

            for (size_t i = 0; i < model->Surfaces.size(); i++) {
                ColladaSurface* surface = model->Surfaces[i];
                //Log::Print(Log::LOG_INFO, "surface %s", surface->Id);
                if (!strcmp(source, surface->Id)) {
                    daesampler->Surface = surface;
                    break;
                }
            }

            free(source);
        }
    }

    model->Samplers.push_back(daesampler);
}

PRIVATE STATIC void COLLADAReader::ParsePhongComponent(ColladaModel* model, ColladaPhongComponent& component, XMLNode* phong) {
    //Log::Print(Log::LOG_INFO, "COLLADAReader::ParsePhongComponent");
    for (size_t i = 0; i < phong->children.size(); i++) {
        XMLNode* node = phong->children[i];

        if (XMLParser::MatchToken(node->name, "color")) {
            //Log::Print(Log::LOG_INFO, "ParsePhongComponent: color array");
            ParseEffectColor(node->children[0], component.Color);
        } else if (XMLParser::MatchToken(node->name, "texture")) {
            char* sampler_name;
            TokenToString(node->attributes.Get("texture"), &sampler_name);

            component.Sampler = nullptr;

            for (size_t j = 0; j < model->Samplers.size(); j++) {
                ColladaSampler* sampler = model->Samplers[j];
                if (!strcmp(sampler->Id, sampler_name)) {
                    //Log::Print(Log::LOG_INFO, "ParsePhongComponent: found sampler %s", sampler->Id);
                    component.Sampler = sampler;
                    break;
                }
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffectColor(XMLNode* contents, float* colors) {
    FloatStringToArray(contents, colors, 4, 1.0f);
}

PRIVATE STATIC void COLLADAReader::ParseEffectFloat(XMLNode* contents, float &floatval) {
    if (contents->name.Length) {
        char* floatstr = (char* )calloc(1, contents->name.Length + 1);
        memcpy(floatstr, contents->name.Start, contents->name.Length);
        floatval = atof(floatstr);
        free(floatstr);
    }
    else
        floatval = 0.0f;
}

PRIVATE STATIC void COLLADAReader::ParseEffectTechnique(ColladaModel* model, ColladaEffect* effect, XMLNode* technique) {
    //Log::Print(Log::LOG_INFO, "COLLADAReader::ParseEffectTechnique");
    for (size_t i = 0; i < technique->children.size(); i++) {
        XMLNode* node = technique->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "specular")) {
            ParsePhongComponent(model, effect->Specular, node);
        } else if (XMLParser::MatchToken(node->name, "ambient")) {
            ParsePhongComponent(model, effect->Ambient, node);
        } else if (XMLParser::MatchToken(node->name, "emission")) {
            ParsePhongComponent(model, effect->Emission, node);
        } else if (XMLParser::MatchToken(node->name, "diffuse")) {
            ParsePhongComponent(model, effect->Diffuse, node);
        } else {
            XMLNode* grandchild = child->children[0];
            if (XMLParser::MatchToken(node->name, "shininess")) {
                ParseEffectFloat(grandchild, effect->Shininess);
            } else if (XMLParser::MatchToken(node->name, "transparency")) {
                ParseEffectFloat(grandchild, effect->Transparency);
            } else if (XMLParser::MatchToken(node->name, "index_of_refraction")) {
                ParseEffectFloat(grandchild, effect->IndexOfRefraction);
            }
        } 
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffect(ColladaModel* model, XMLNode* parent, XMLNode* effect) {
    ColladaEffect* daeeffect = new ColladaEffect;

    memset(daeeffect, 0x00, sizeof(ColladaEffect));

    TokenToString(parent->attributes.Get("id"), &daeeffect->Id);
    //Log::Print(Log::LOG_INFO, "Effect name: %s", daeeffect->Id);

    for (size_t i = 0; i < effect->children.size(); i++) {
        XMLNode* node = effect->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "newparam")) {
            if (XMLParser::MatchToken(child->name, "surface")) {
                ParseSurface(model, node, child);
            } else if (XMLParser::MatchToken(child->name, "sampler2D")) {
                ParseSampler(model, node, child);
            }
        } else if (XMLParser::MatchToken(node->name, "technique")) {
            ParseEffectTechnique(model, daeeffect, child);
        }
    }

    model->Effects.push_back(daeeffect);
}

PRIVATE STATIC void COLLADAReader::ParseMaterial(ColladaModel* model, XMLNode* parent, XMLNode* material) {
    ColladaMaterial* daematerial = new ColladaMaterial;

    daematerial->Effect = nullptr;

    TokenToString(parent->attributes.Get("id"), &daematerial->Id);
    TokenToString(parent->attributes.Get("name"), &daematerial->Name);
    
    //Log::Print(Log::LOG_INFO, "Material id: %s", daematerial->Id);
    //Log::Print(Log::LOG_INFO, "Material name: %s", daematerial->Name);

    TokenToString(material->attributes.Get("url"), &daematerial->EffectLink);
    //Log::Print(Log::LOG_INFO, "Material URL: %s", daematerial->EffectLink);

    model->Materials.push_back(daematerial);
}

PRIVATE STATIC void COLLADAReader::AssignEffectsToMaterials(ColladaModel* model) {
    for (size_t i = 0; i < model->Materials.size(); i++) {
        ColladaMaterial* material = model->Materials[i];
        //Log::Print(Log::LOG_INFO, "material %s", material->Name);

        for (size_t j = 0; j < model->Effects.size(); j++) {
            ColladaEffect* effect = model->Effects[j];
            //Log::Print(Log::LOG_INFO, "effect %s", effect->Id);
            if (!strcmp((material->EffectLink + 1), effect->Id)) {
                //Log::Print(Log::LOG_INFO, "Assigned effect %s to material %s", effect->Id, material->Name);
                material->Effect = effect;
                break;
            }
        }
    }
}

static void InstanceMesh(ColladaModel* model, ColladaNode* daenode, char* geometry_url)
{
    //Log::Print(Log::LOG_INFO, "InstanceMesh: URL %s, %d geometries", geometry_url, model->Geometries.size());

    for (size_t i = 0; i < model->Geometries.size(); i++) {
        ColladaGeometry* geometry = model->Geometries[i];
        //Log::Print(Log::LOG_INFO, "geometry %s", geometry->Name);
        if (!strcmp(geometry_url, geometry->Name)) {
            //Log::Print(Log::LOG_INFO, "Instanced mesh %s", geometry_url);
            daenode->Mesh = geometry->Mesh;
            break;
        }
    }
}

static void InstanceMaterial(ColladaModel* model, ColladaNode* daenode, XMLNode* node)
{
    char *target;
    TokenToString(node->attributes.Get("target"), &target);

    //Log::Print(Log::LOG_INFO, "InstanceMaterial %s", target);

    for (size_t i = 0; i < model->Materials.size(); i++) {
        ColladaMaterial* material = model->Materials[i];
        //Log::Print(Log::LOG_INFO, "%s", material->Id);
        if (!strcmp((target + 1), material->Id)) {
            //Log::Print(Log::LOG_INFO, "Instanced material %s", target);
            daenode->Material = material;
            break;
        }
    }

    free(target);
}

static void InstanceFromXML(ColladaModel* model, ColladaNode* daenode, XMLNode* node) {
    if (XMLParser::MatchToken(node->name, "instance_geometry")) {
        char *geometry_url;
        TokenToString(node->attributes.Get("url"), &geometry_url);

        InstanceMesh(model, daenode, (geometry_url + 1));

        free(geometry_url);
    } else if (XMLParser::MatchToken(node->name, "instance_material")) {
        InstanceMaterial(model, daenode, node);
    }
}

PRIVATE STATIC void COLLADAReader::ParseNode(ColladaModel* model, XMLNode* node) {
    ColladaNode* daenode = new ColladaNode;

    TokenToString(node->attributes.Get("id"), &daenode->Id);
    TokenToString(node->attributes.Get("name"), &daenode->Name);

    daenode->Mesh = nullptr;

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        //Log::Print(Log::LOG_INFO, "ParseNode: node child %d", i);

        if (XMLParser::MatchToken(child->name, "matrix")) {
            FloatStringToArray(child->children[0], daenode->Matrix, 16, 0.0f);
        } else if (XMLParser::MatchToken(child->name, "instance_controller")) {
            // Controllers don't have too much things on them to warrant a new function
            for (size_t j = 0; j < child->children.size(); j++) {
                XMLNode* grandchild = child->children[j];

                //Log::Print(Log::LOG_INFO, "ParseNode: controller grandchild %d", j);

                if (XMLParser::MatchToken(grandchild->name, "skeleton")) {
                    // unimplemented
                } else if (XMLParser::MatchToken(grandchild->name, "bind_material")) {
                    XMLNode* common = grandchild->children[0];
                    if (common && XMLParser::MatchToken(common->name, "technique_common")) {
                        for (size_t k = 0; k < common->children.size(); k++)
                            InstanceFromXML(model, daenode, common->children[k]);
                    }
                }
            }
        } else {
            InstanceFromXML(model, daenode, child);
        }
    }

    // This is actually fucked. It makes no sense.
    // Okay, so nodes are part of scenes.
    // A node can instantiate a mesh, with instance_geometry.
    // Except that some models decide NOT to do that, and
    // put the mesh to be loaded in the "name" attribute
    // of the nodes. Why?!
    if (daenode->Mesh == nullptr) {
        InstanceMesh(model, daenode, daenode->Name);
        //Log::Print(Log::LOG_INFO, "Instanced mesh for %s late", daenode->Name);
    }

    // Assign material to mesh
    if (daenode->Mesh != nullptr && (daenode->Mesh->Material == nullptr))
    {
        //Log::Print(Log::LOG_INFO, "Assigned material to mesh");
        daenode->Mesh->Material = daenode->Material;
    }

    model->Nodes.push_back(daenode);
}

PRIVATE STATIC void COLLADAReader::ParseVisualScene(ColladaModel* model, XMLNode* scene) {
    ColladaScene* daescene = new ColladaScene;

    //Log::Print(Log::LOG_INFO, "ParseVisualScene: %d nodes", scene->children.size());

    for (size_t i = 0; i < scene->children.size(); i++) {
        XMLNode* child = scene->children[i];
        if (XMLParser::MatchToken(child->name, "node")) {
            ParseNode(model, child);
        }
    }

    model->Scenes.push_back(daescene);
}

PRIVATE STATIC void COLLADAReader::ParseLibrary(ColladaModel* model, XMLNode* library) {
    //Log::Print(Log::LOG_INFO, "COLLADAReader::ParseLibrary");
    for (size_t i = 0; i < library->children.size(); i++) {
        XMLNode* node = library->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "geometry")) {
            //Log::Print(Log::LOG_INFO, "Parsing geometry...");
            ParseGeometry(model, node);
        } else if (XMLParser::MatchToken(node->name, "material")) {
            //Log::Print(Log::LOG_INFO, "Parsing material...");
            ParseMaterial(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "effect")) {
            //Log::Print(Log::LOG_INFO, "Parsing effect...");
            ParseEffect(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "image")) {
            //Log::Print(Log::LOG_INFO, "Parsing image...");
            ParseImage(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "visual_scene")) {
            //Log::Print(Log::LOG_INFO, "Parsing visual scene...");
            ParseVisualScene(model, node); // not child, that's whatever the first collada node is
        }
    }
}

//
// CONVERSION
//

PRIVATE STATIC void COLLADAReader::ProcessPrimitive(IModel* imodel, int meshIndex,
    ColladaInput* input, int prim,
    vector<int>& triIndexes, vector<int>& texIndexes, vector<int>& nrmIndexes, vector<int>& colIndexes) 
{
    ColladaMeshAccessor* accessor = input->Source->Accessors[0];
    vector<float> values = accessor->Source->Contents;

    int stride = accessor->Stride;
    int idx = (prim * stride);
    int vecsize = values.size();

    if (!strcmp(input->Semantic, "POSITION")) {
        // Resize vertex list to fit
        if (imodel->Vertices[meshIndex].size() < vecsize)
            imodel->Vertices[meshIndex].resize(vecsize);

        // Insert vertex
        imodel->Vertices[meshIndex][prim] = IVertex(values[idx], values[idx + 1], values[idx + 2]);

        // Store tri's vertex index
        triIndexes.push_back(prim);
    } else if (!strcmp(input->Semantic, "NORMAL")) {
        // Resize normal list to fit
        if (imodel->Normals[meshIndex].size() < vecsize)
            imodel->Normals[meshIndex].resize(vecsize);

        // Insert normal
        imodel->Normals[meshIndex][prim] = IVertex(values[idx], values[idx + 1], values[idx + 2]);

        // Store tri's normal index
        nrmIndexes.push_back(prim);
    } else if (!strcmp(input->Semantic, "TEXCOORD")) {
        // Resize tex list to fit
        if (imodel->UVs[meshIndex].size() < vecsize)
            imodel->UVs[meshIndex].resize(vecsize);

        // Insert tex
        imodel->UVs[meshIndex][prim] = ITexCoord(values[idx], values[idx + 1]);

        // Store tri's tex index
        texIndexes.push_back(prim);
    } else if (!strcmp(input->Semantic, "COLOR")) {
        // Resize color list to fit
        if (imodel->Colors[meshIndex].size() < vecsize)
            imodel->Colors[meshIndex].resize(vecsize);

        // Insert color
        imodel->Colors[meshIndex][prim] = IColor(values[idx], values[idx + 1], values[idx + 2]);

        // Store tri's color index
        colIndexes.push_back(prim);
    }
}

PRIVATE STATIC void COLLADAReader::DoConversion(ColladaModel* daemodel, IModel* imodel) {
    //Log::Print(Log::LOG_INFO, "COLLADAReader::DoConversion");

    int numMeshes = daemodel->Meshes.size();
    imodel->MeshCount = numMeshes;
    imodel->Faces.resize(numMeshes);
    imodel->Vertices.resize(numMeshes);
    imodel->Normals.resize(numMeshes);
    imodel->UVs.resize(numMeshes);
    imodel->Colors.resize(numMeshes);
    imodel->Materials.resize(numMeshes);
    imodel->FaceCount.resize(numMeshes);

    for (int meshIndex = 0; meshIndex < numMeshes; meshIndex++)
        imodel->FaceCount[meshIndex] = 0;

    //Log::Print(Log::LOG_INFO, "daemodel->Meshes.size(): %d", daemodel->Meshes.size());
    for (int meshNum = 0; meshNum < daemodel->Meshes.size(); meshNum++) {
        ColladaMesh* mesh = daemodel->Meshes[meshNum];

        int numInputs = mesh->Triangles.Inputs.size();

        vector<int> triIndexes;
        vector<int> texIndexes;
        vector<int> nrmIndexes;
        vector<int> colIndexes;

        int numVertices = 0;
        int numProcessedInputs = 0;

        triIndexes.resize(3);
        texIndexes.resize(3);
        nrmIndexes.resize(3);
        colIndexes.resize(3);

        triIndexes.clear();
        texIndexes.clear();
        nrmIndexes.clear();
        colIndexes.clear();

        //Log::Print(Log::LOG_INFO, "mesh->Triangles.Primitives.size(): %d", mesh->Triangles.Primitives.size());
        //Log::Print(Log::LOG_INFO, "numInputs: %d", numInputs);

        for (int primitiveInput = 0; primitiveInput < mesh->Triangles.Primitives.size(); primitiveInput++) {
            int prim = mesh->Triangles.Primitives[primitiveInput];

            ColladaInput* input = nullptr;

            // Find matching input.
            if (numInputs > 1) {
                for (int inputIndex = 0; inputIndex < numInputs; inputIndex++) {
                    ColladaInput* thisInput = mesh->Triangles.Inputs[inputIndex];
                    if ((primitiveInput % numInputs) == thisInput->Offset) {
                        input = thisInput;
                        break;
                    }
                }
            } else
                input = mesh->Triangles.Inputs[0];

            // ?!
            if (input == nullptr)
                continue; 

            // VERTEX input
            if (input->Children.size()) {
                //Log::Print(Log::LOG_INFO, "VERTEX input!");
                for (int inputIndex = 0; inputIndex < input->Children.size(); inputIndex++) {
                    ColladaInput* childInput = input->Children[inputIndex];
                    //Log::Print(Log::LOG_INFO, "Processing VERTEX input! %s", childInput->Semantic);
                    ProcessPrimitive(imodel, meshNum, childInput, prim, triIndexes, texIndexes, nrmIndexes, colIndexes);

                    // Count each child input.
                    numProcessedInputs++;
                }

                // A VERTEX input counts as a vertex, obviously
                numVertices++;
            } else {
                ProcessPrimitive(imodel, meshNum, input, prim, triIndexes, texIndexes, nrmIndexes, colIndexes);

                // Count each processed input.
                numProcessedInputs++;

                // When enough inputs have been processed,
                // it counts as a vertex.
                if ((numProcessedInputs % numInputs) == 0)
                    numVertices++;
            }

            // When three vertices have been added, add a face.
            if (numVertices == 3) {
                /*
                Log::Print(Log::LOG_INFO, "add tri %d: vtx %d %d %d tex %d %d %d nrm %d %d %d col %d %d %d",
                    imodel->FaceCount[meshNum],
                    triIndexes[0], triIndexes[1], triIndexes[2],
                    texIndexes[0], texIndexes[1], texIndexes[2],
                    nrmIndexes[0], nrmIndexes[1], nrmIndexes[2],
                    colIndexes[0], colIndexes[1], colIndexes[2]
                    );
                */

                imodel->Faces[meshNum].push_back(IFace(
                    triIndexes[0], triIndexes[1], triIndexes[2],
                    texIndexes[0], texIndexes[1], texIndexes[2],
                    nrmIndexes[0], nrmIndexes[1], nrmIndexes[2],
                    colIndexes[0], colIndexes[1], colIndexes[2]
                    ));

                numVertices = 0;
                triIndexes.clear();
                texIndexes.clear();
                nrmIndexes.clear();
                colIndexes.clear();

                imodel->FaceCount[meshNum]++;
            }
        }

        IMaterial mat = IMaterial();
        mat.Tex = nullptr;

        if (mesh->Material && mesh->Material->Effect)
        {
            ColladaEffect* effect = mesh->Material->Effect;

            for (int i = 0; i < 4; i++)
            {
                mat.Specular[i] = effect->Specular.Color[i];
                mat.Ambient[i] = effect->Ambient.Color[i];
                mat.Emission[i] = effect->Emission.Color[i];
                mat.Diffuse[i] = effect->Diffuse.Color[i];
            }

            mat.Shininess = effect->Shininess;
            mat.Transparency = effect->Transparency;
            mat.IndexOfRefraction = effect->IndexOfRefraction;

            //Log::Print(Log::LOG_INFO, "Shininess: %f", mat.Shininess);
            //Log::Print(Log::LOG_INFO, "Transparency: %f", mat.Transparency);
            //Log::Print(Log::LOG_INFO, "Index of refraction: %f", mat.IndexOfRefraction);

            ColladaSampler* sampler = effect->Diffuse.Sampler;
            if (sampler && sampler->Surface && sampler->Surface->Image)
                mat.Tex = new Image(sampler->Surface->Image->Path);
        }

        imodel->Materials[meshNum] = mat;
    }
    //Log::Print(Log::LOG_INFO, "done");
}

//
// MAIN PARSER
//

PUBLIC STATIC IModel* COLLADAReader::Convert(const char* sourceF) {
    XMLNode* modelXML = XMLParser::ParseFromResource(sourceF);
    if (!modelXML) {
        //Log::Print(Log::LOG_ERROR, "Could not read model from resource \"%s\"", sourceF);
        return nullptr;
    }

    IModel* model = new IModel();
    ColladaModel daemodel;

    const char* matchAsset = "asset";
    const char* matchLibrary = "library_";

    //Log::Print(Log::LOG_INFO, "Reading model %s", sourceF);
    if (modelXML->children[0])
        modelXML = modelXML->children[0];

    //Log::Print(Log::LOG_INFO, "%d", modelXML->children.size());
    for (size_t i = 0; i < modelXML->children.size(); i++) {
        Token section_token = modelXML->children[i]->name;
        char* section_name = (char* )calloc(1, section_token.Length + 1);
        memcpy(section_name, section_token.Start, section_token.Length);

        // Parse asset
        //Log::Print(Log::LOG_INFO, "%s", section_name);
        if (!strcmp(section_name, matchAsset))
            ParseAsset(&daemodel, modelXML->children[i]);
        // Parse library
        else if (!strncmp(section_name, matchLibrary, strlen(matchLibrary)))
            ParseLibrary(&daemodel, modelXML->children[i]);

        free(section_name);
    }

    // Assign effects to materials
    AssignEffectsToMaterials(&daemodel);

    // Convert to IModel
    DoConversion(&daemodel, model);

    return model;
}
