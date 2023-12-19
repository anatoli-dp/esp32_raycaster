#include <unordered_map>

#include "./display.h"

ImageBuffer decodePNG(const byte* buffer, int size);

static std::unordered_map<int, TFT_eSprite> consoleSprites;

//game sprite limits
static const int maxSpriteBytes = 1048576;//1mb available for loading game sprites . . .
static int currentSpriteBytes = 0;

static Datum spriteDatum = TOP_LEFT;
static uint16_t spriteAlpha = TFT_BLACK;
static bool useAlpha = false;

void bb_spralign (Datum datum)
{
    spriteDatum = datum;
}

void bb_spralpha (bb_color color)
{
    spriteAlpha = convertColor(color);
}

void bb_sprusealpha (bool use)
{
    useAlpha = use;
}

static bool hasEntry (int sprNum)
{
    std::unordered_map<int, TFT_eSprite>::const_iterator entry = consoleSprites.find(sprNum);
    return !(entry == consoleSprites.end());
}

TFT_eSprite* bb_sprget (int sprNum)
{
    if (hasEntry(sprNum)) {
        return &(consoleSprites[sprNum]);
    } else {
        return nullptr;
    }
}

SpriteStatus bb_sprnew (int sprNum, int width, int height)
{
    if (!hasEntry(sprNum)) {
        void* ptr;
        if (sprNum >= 0) {
            consoleSprites[sprNum] = TFT_eSprite();
            consoleSprites[sprNum].setAttribute(PSRAM_ENABLE, true);
            consoleSprites[sprNum].setColorDepth(16);
            ptr = consoleSprites[sprNum].createSprite(width, height);
        } else {
            consoleSprites[sprNum] = TFT_eSprite();
            consoleSprites[sprNum].setAttribute(PSRAM_ENABLE, true);
            consoleSprites[sprNum].setColorDepth(16);
            ptr = consoleSprites[sprNum].createSprite(width, height);
        }

        return ((ptr == nullptr) ? SPRITE_ERROR_MEMORY : SPRITE_OK);
    } else {
        return SPRITE_ERROR_DUPLICATE;
    }
}

SpriteStatus bb_sprload (int sprNum, const byte* buffer, int size)
{
    if (!hasEntry(sprNum)) {
        ImageBuffer ib = decodePNG(buffer, size);
        if (ib.status == FILE_OK) {
            bb_sprnew(sprNum, ib.width, ib.height);
            bb_sprget(sprNum)->pushImage(0, 0, ib.width, ib.height, (uint16_t*)ib.buffer);
            free(ib.buffer);

            return SPRITE_OK;
        } else {
            return SPRITE_ERROR_MEMORY;
        }
    } else {
        return SPRITE_ERROR_DUPLICATE;
    }
}

SpriteStatus bb_sprload (int sprNum, char* path)
{
    if (!hasEntry(sprNum)) {
        ImageBuffer ib = bb_storage_image(path);
        if (ib.status == FILE_OK) {
            void* ptr;

            if (sprNum >= 0) {
                bb_sprnew(sprNum, ib.width, ib.height);

                ptr = bb_sprget(sprNum);
                if (ptr == nullptr)
                {
                    free(ib.buffer);
                    return SPRITE_ERROR_MEMORY;
                }

                bb_sprget(sprNum)->pushImage(0, 0, ib.width, ib.height, (uint16_t*)ib.buffer);
                free(ib.buffer);

                return SPRITE_OK;
            } else {
                bb_sprnew(sprNum, ib.width, ib.height);

                ptr = bb_sprget(sprNum);
                if (ptr == nullptr)
                {
                    free(ib.buffer);
                    return SPRITE_ERROR_MEMORY;
                }

                bb_sprget(sprNum)->pushImage(0, 0, ib.width, ib.height, (uint16_t*)ib.buffer);
                free(ib.buffer);

                return SPRITE_OK;
            }
        } else {
            return SPRITE_ERROR_MEMORY;
        }
    } else {
        return SPRITE_ERROR_DUPLICATE;
    }
}

void bb_sprfree ()
{
    for (auto & [sprNum, spr] : consoleSprites)
    {
        if (sprNum >= 0)
        {
            spr.deleteSprite();
            consoleSprites.erase(sprNum);
        }
    }
}

void bb_sprfree (int sprNum)
{
    if (hasEntry(sprNum))
    {
        consoleSprites[sprNum].deleteSprite();
        consoleSprites.erase(sprNum);
    }
}

void bb_spr (int sprNum, int x, int y)
{
    int posX;
    int posY;

    TFT_eSprite* sprite;
    TFT_eSprite* bufferSprite = bb_displaybuffer();

    if (hasEntry(sprNum))
    {
        sprite = bb_sprget(sprNum);

        int spriteWidth = sprite->width();
        int spriteHeight = sprite->height();

        switch (spriteDatum)
        {
            case TOP_LEFT:
                posX = x;
                posY = y;
                break;
            case TOP_CENTER:
                posX = x - (spriteWidth / 2);
                posY = y;
                break;
            case TOP_RIGHT:
                posX = x - spriteWidth;
                posY = y;
                break;
            case CENTER_LEFT:
                posX = x;
                posY = y - (spriteHeight / 2);
                break;
            case CENTER_CENTER:
                posX = x - (spriteWidth / 2);
                posY = y - (spriteHeight / 2);
                break;
            case CENTER_RIGHT:
                posX = x - spriteWidth;
                posY = y - (spriteHeight / 2);
                break;
            case BOTTOM_LEFT:
                posX = x;
                posY = y - spriteHeight;
                break;
            case BOTTOM_CENTER:
                posX = x - (spriteWidth / 2);
                posY = y - spriteHeight;
                break;
            case BOTTOM_RIGHT:
                posX = x - spriteWidth;
                posY = y - spriteHeight;
                break;
        }

        //check if sprite and window intersect
        //(aX < (bX + bLen) && (aX + aLen) > bX) && (aY < (bY - bLen) && (aY - aLen) > bY)
        if ((0 < (posX + spriteWidth) && (0 + BB_DISPLAY_WIDTH) > posX) && (0 < (posY + spriteHeight) && (0 + BB_DISPLAY_HEIGHT) > posY))
        {
            if (useAlpha)
                sprite->pushToSprite(bufferSprite, posX, posY, spriteAlpha);
            else 
                sprite->pushToSprite(bufferSprite, posX, posY);
        }
    }
}