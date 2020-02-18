#ifndef FONTFACE_H
#define FONTFACE_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL


#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/ISprite.h>

class FontFace {
public:
    static ISprite* SpriteFromFont(Stream* stream, int pixelSize, char* filename);
};

#endif /* FONTFACE_H */