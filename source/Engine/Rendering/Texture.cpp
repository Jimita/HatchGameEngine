#if INTERFACE
#include <Engine/Includes/Standard.h>

need_t Texture;

class Texture {
public:
    Uint32   Format;
    Uint32   Access;
    Uint32   Width;
    Uint32   Height;

    void*    Pixels;
    int      Pitch;

    Uint32   ID;
    void*    DriverData;

    Texture* Prev;
    Texture* Next;
};
#endif

#include <Engine/Rendering/Texture.h>

#include <Engine/Diagnostics/Memory.h>

PUBLIC STATIC Texture* Texture::New(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = (Texture*)Memory::TrackedCalloc("Texture::Texture", 1, sizeof(Texture));
    texture->Format = format;
    texture->Access = access;
    texture->Width = width;
    texture->Height = height;
    return texture;
}
// PUBLIC STATIC Texture* Texture::FromFilename(const char* filename) {
//     Texture* tex;
//     SDL_Surface* temp = IMG_Load(filename);
//     tex = Application::RendererPtr->CreateTextureFromSurface(temp);
//     SDL_FreeSurface(temp);
//     return tex;
// }
