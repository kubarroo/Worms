#include "Core/Utils.h"
#include <gtest/gtest.h>

constexpr SDL_PixelFormat format = SDL_PIXELFORMAT_ARGB8888;

namespace
{
TEST(ColliderFromSprite, PaddedRowsAndTransparentColoredPixels)
{
    Uint32 padded[] = {0x01000000, 0x01000000, 0xFFFFFFFF,
                       0x01000000, 0x01000000, 0xFFFFFFFF};
    auto shapes = MarchingSquares(padded, 2, 2, 0, 3);
    ASSERT_EQ(shapes.size(), 1);
    EXPECT_EQ(shapes.front().size(), 8);
    Uint32 transparent[] = {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF};
    EXPECT_TRUE(MarchingSquares(transparent, 2, 2, 0).empty());
}

TEST(ColliderFromSprite, MarchingSquares_Empty2x2)
{
    Uint32 pixels[4];

    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            pixels[y * 2 + x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
        }
    }

    auto result = MarchingSquares(pixels, 2, 2, 0);
    EXPECT_EQ(result.size(), 0);
}

TEST(ColliderFromSprite, MarchingSquares_Full2x2)
{
    Uint32 pixels[4];
    SDL_Point validation[8] = {{1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}, {0, 0}};

    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            pixels[y * 2 + x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
        }
    }

    auto result = MarchingSquares(pixels, 2, 2, 0);
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].size(), 8u);

    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result[0][i].x, validation[i].x);
        EXPECT_EQ(result[0][i].y, validation[i].y);
    }
}

TEST(ColliderFromSprite, MarchingSquares_Diagonal2x2)
{
    Uint32 pixels[4];
    SDL_Point validation[8] = {{1, 0}, {1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}, {0, 1}, {0, 0}};

    pixels[0] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[1] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[2] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[3] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    auto result = MarchingSquares(pixels, 2, 2, 0);
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].size(), 8u);

    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result[0][i].x, validation[i].x);
        EXPECT_EQ(result[0][i].y, validation[i].y);
    }
}

TEST(ColliderFromSprite, MarchingSquares_Empty4x4)
{
    Uint32 pixels[16];

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            pixels[y * 4 + x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
        }
    }

    auto result = MarchingSquares(pixels, 4, 4, 0);
    EXPECT_EQ(result.size(), 0);
}

TEST(ColliderFromSprite, MarchingSquares_Shape4x4)
{
    Uint32 pixels[16];

    // ###
    //  ###
    // ##
    //  ##

    SDL_Point validation[] = {{1, 0}, {2, 0}, {3, 0}, {3, 1}, {4, 1}, {4, 2}, {3, 2},
                              {2, 2}, {2, 3}, {3, 3}, {3, 4}, {2, 4}, {1, 4}, {1, 3},
                              {0, 3}, {0, 2}, {1, 2}, {1, 1}, {0, 1}, {0, 0}};
    pixels[0] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[1] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[2] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[3] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);

    pixels[4] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[5] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[6] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[7] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    pixels[8] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[9] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[10] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[11] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);

    pixels[12] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[13] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[14] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[15] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);

    auto result = MarchingSquares(pixels, 4, 4, 0);
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].size(), 20u);

    for (int i = 0; i < 20; i++)
    {
        EXPECT_EQ(result[0][i].x, validation[i].x);
        EXPECT_EQ(result[0][i].y, validation[i].y);
    }
}

TEST(ColliderFromSprite, MarchingSquares_Hole4x4)
{
    Uint32 pixels[16];

    // ####
    // #  #
    // #  #
    // ####

    SDL_Point validation1[] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {4, 1}, {4, 2}, {4, 3}, {4, 4},
                               {3, 4}, {2, 4}, {1, 4}, {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}};

    SDL_Point validation2[] = {{1, 2}, {1, 3}, {2, 3}, {3, 3}, {3, 2}, {3, 1}, {2, 1}, {1, 1}};
    pixels[0] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[1] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[2] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[3] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    pixels[4] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[5] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[6] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[7] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    pixels[8] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[9] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[10] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 0);
    pixels[11] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    pixels[12] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[13] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[14] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);
    pixels[15] = SDL_MapRGBA(SDL_GetPixelFormatDetails(format), nullptr, 0, 0, 0, 255);

    auto result = MarchingSquares(pixels, 4, 4, 0);
    ASSERT_EQ(result.size(), 2u);
    ASSERT_EQ(result[0].size(), 16u);
    ASSERT_EQ(result[1].size(), 8u);

    for (int i = 0; i < 16; i++)
    {
        EXPECT_EQ(result[0][i].x, validation1[i].x);
        EXPECT_EQ(result[0][i].y, validation1[i].y);
    }

    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result[1][i].x, validation2[i].x);
        EXPECT_EQ(result[1][i].y, validation2[i].y);
    }
}
} // namespace
