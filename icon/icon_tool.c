
#include <stdint.h>
#include <stdio.h>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define alen(x) (sizeof(x) / sizeof((x)[0]))

int write_entire_file(char *path, uint8_t *data, int64_t data_size)
{
    FILE *file = fopen(path, "wb");
    if (!file) return(0);
    size_t written = fwrite(data, 1, data_size, file);
    if (written != data_size) return(0);
    fclose(file);
    return(1);
}

typedef struct icon icon;
struct icon
{
    int Size;
    int PngEncode;

    uint8_t *Bytes;
    int64_t ByteCount;

    uint8_t *PngData;
    int32_t PngLength;
};

#pragma pack(push, 1)
typedef struct ico_header ico_header;
struct ico_header
{
    uint16_t Reserved;
    uint16_t Type;
    uint16_t ImageCount;
};

typedef struct ico_header_entry ico_header_entry;
struct ico_header_entry
{
    uint8_t Width;
    uint8_t Height;
    uint8_t ColorPalleteSize;
    uint8_t Reserved;
    uint16_t ColorPlanes;
    uint16_t BitsPerPixel;
    uint32_t DataSize;
    uint32_t OffsetInFile;
};

typedef struct dib_header dib_header;
struct dib_header // BITMAPINFOHEADER
{
    uint32_t HeaderSize;
    int32_t Width;
    int32_t Height;
    uint16_t ColorPlanes;
    uint16_t BitsPerPixel;
    uint32_t CompressionMethod;
    uint32_t ImageSize;
    int32_t HorizontalResolution;
    int32_t VerticalResolution;
    uint32_t ColorPaletteSize;
    uint32_t ImportantColors;
};

typedef struct res_header res_header;
struct res_header
{
    uint32_t DataSize;
    uint32_t HeaderSize;
    uint32_t Type;
    uint32_t Name;
    uint32_t Version1;
    uint16_t MemoryFlags;
    uint16_t LanguageId;
    uint32_t Version2;
    uint32_t Characteristics;
};

typedef struct res_icon_group_a res_icon_group_a;
struct res_icon_group_a
{
    uint32_t DataSize;
    uint32_t HeaderSize;
    uint32_t Type;
};
typedef struct res_icon_group_b res_icon_group_b;
struct res_icon_group_b
{
    uint32_t Version;
    uint16_t MemoryFlags;
    uint16_t LanguageId;
    uint32_t Version2;
    uint32_t Characteristics;
    uint16_t Reserved1;
    uint16_t GroupType;
    uint16_t GroupMemberCount;
};

typedef struct res_header_icon_group_entry res_header_icon_group_entry;
struct res_header_icon_group_entry
{
    uint8_t x;
    uint8_t y;
    uint8_t ColorCount;
    uint8_t Reserved1;
    uint16_t Planes;
    uint16_t BitsPerPixel;
    uint32_t DataBytes;
    uint16_t Id;
};

enum { RES_HEADER_SIZE = 32,
       RES_ICON_GROUP_UNCOUNTED = 6,
       RES_HEADER_ICON_GROUP_ENTRY_SIZE = 14,
       RES_ICON_TYPE = 3,
       RES_ICON_GROUP_TYPE = 14,
       RES_INDEX_MASK = 0x0000ffff,
       RES_INDEX_SHIFT = 16,
       RES_DEFAULT_MEMORY_FLAGS = 0x1010, // According to docs this isn't read anyways
       RES_ICON_GROUP_MEMORY_FLAGS = 0x1030,
       RES_DEFAULT_LANGUAGE_ID = 0x409,
       RES_ICON_GROUP_ICON_TYPE = 1, };
static_assert(RES_HEADER_SIZE == sizeof(res_header), "bruh");
#pragma pack(pop)

void ImageSwapForWindows(uint8_t *Data, int32_t Dimensions)
{
    for (int32_t Pixel = 0; Pixel < Dimensions*Dimensions; Pixel += 1) {
        uint8_t Temp = Data[Pixel*4 + 2];
        Data[Pixel*4 + 2] = Data[Pixel*4 + 0];
        Data[Pixel*4 + 0] = Temp;
    }
    for (int32_t Row = 0; Row < Dimensions/2; Row += 1) {
        uint8_t *A = &Data[Row*Dimensions * sizeof(uint32_t)];
        uint8_t *B = &Data[(Dimensions - Row - 1)*Dimensions * sizeof(uint32_t)];
        for (int32_t j = 0; j < Dimensions*sizeof(uint32_t); j += 1) {
            uint8_t Temp = *A;
            *A = *B;
            *B = Temp;

            A++;
            B++;
        }
    }
}

void WriteIco(icon *Icons, int32_t IconCount, char *IcoPath)
{
    int64_t TotalSize = 0;
    for (int32_t i = 0; i < IconCount; i += 1) {
        if (Icons[i].PngEncode) {
            TotalSize += (Icons[i].PngLength + 3) / 4 * 4;
        } else {
            TotalSize += Icons[i].ByteCount + sizeof(dib_header);
        }
    }
    TotalSize += sizeof(ico_header) + IconCount*sizeof(ico_header_entry);
    TotalSize += 16*IconCount; // For padding if we need to round up memory locations anywhere
    uint8_t *FileContent = (uint8_t *) malloc(TotalSize);
    memset(FileContent, 0, TotalSize);

    ico_header *Header = (ico_header *) FileContent;
    Header->Reserved = 0;
    Header->Type = 1;
    Header->ImageCount = IconCount;

    ico_header_entry *HeaderEntries = (ico_header_entry *) (FileContent + sizeof(ico_header));
    int64_t FileOffset = sizeof(ico_header) + IconCount*sizeof(ico_header_entry);

    for (int32_t i = IconCount - 1; i >= 0; i -= 1) {
        int64_t OldFileOffset = FileOffset;
        FileOffset = (FileOffset + 15) / 16 * 16;
        memset(FileContent + OldFileOffset, 0, FileOffset - OldFileOffset);

        ico_header_entry *Entry = &HeaderEntries[i];
        Entry->Width = Icons[i].Size;
        Entry->Height = Icons[i].Size;
        Entry->ColorPalleteSize = 0;
        Entry->Reserved = 0;
        Entry->ColorPlanes = 1;
        Entry->BitsPerPixel = 32;
        Entry->OffsetInFile = FileOffset;

        if (Icons[i].PngEncode) {
            Entry->DataSize = Icons[i].PngLength;
            memcpy(&FileContent[FileOffset], Icons[i].PngData, Icons[i].PngLength);
            FileOffset += (Icons[i].PngLength + 3) / 4 * 4;
        } else {
            Entry->DataSize = Icons[i].ByteCount + sizeof(dib_header);

            dib_header *DibHeader = (dib_header *) &FileContent[FileOffset];
            FileOffset += sizeof(dib_header);
            memcpy(&FileContent[FileOffset], Icons[i].Bytes, Icons[i].ByteCount);
            ImageSwapForWindows(&FileContent[FileOffset], Icons[i].Size);
            FileOffset += Icons[i].ByteCount;

            memset((uint8_t *) DibHeader, 0, sizeof(dib_header));
            DibHeader->HeaderSize = sizeof(dib_header);
            DibHeader->Width = Icons[i].Size;
            DibHeader->Height = Icons[i].Size*2;
            DibHeader->ColorPlanes = 1;
            DibHeader->BitsPerPixel = 32;
            DibHeader->ImageSize = Icons[i].ByteCount;
        }
    }

    if (!write_entire_file(IcoPath, FileContent, TotalSize)) {
        printf("Couldn't save to \"%s\"\n", IcoPath);
        exit(1);
    }
}

void WriteRes(icon *Icons, int32_t IconCount, char *ResourcePath, char *ResourceName)
{
    for (int32_t i = 0; i < IconCount; i += 1) {
        if (Icons[i].Size > 256) {
            printf("Can't have icons larger than 256^2\n");
            exit(1);
        }
    }


    int ResourceNameLength = strlen(ResourceName);
    wchar_t WideResourceName[256];
    for (int i = 0; i < ResourceNameLength; ++i) WideResourceName[i] = ResourceName[i];
    WideResourceName[ResourceNameLength] = 0;

    int32_t WideResourceNameSpace = ((ResourceNameLength + 1)*sizeof(wchar_t) + 3) / 4 * 4;

    int64_t TotalSize = 0;
    for (int32_t i = 0; i < IconCount; i += 1) {
        if (Icons[i].PngEncode) {
            TotalSize += (Icons[i].PngLength + 3) / 4 * 4;
        } else {
            TotalSize += Icons[i].ByteCount + sizeof(dib_header);
        }
    }
    TotalSize += sizeof(res_header) * (IconCount + 1); // +1 for null header
    TotalSize += sizeof(res_icon_group_a) + sizeof(res_icon_group_b) + WideResourceNameSpace +
                 (IconCount * sizeof(res_header_icon_group_entry));
    TotalSize = (TotalSize + 3) / 4 * 4;
    uint8_t *FileContent = (uint8_t *) malloc(TotalSize);
    memset(FileContent, 0, TotalSize);

    int64_t FileOffset = 0;
    res_header *NullHeader = (res_header *) &FileContent[FileOffset];
    FileOffset += sizeof(res_header);

    NullHeader->HeaderSize = sizeof(res_header);
    NullHeader->Type = RES_INDEX_MASK;
    NullHeader->Name = RES_INDEX_MASK;

    for (int32_t i = 0; i < IconCount; i += 1) {
        icon *Icon = &Icons[i];

        res_header *Header = (res_header *) &FileContent[FileOffset];
        FileOffset += sizeof(res_header);

        Header->HeaderSize = sizeof(res_header);
        Header->Type = RES_INDEX_MASK | (RES_ICON_TYPE << RES_INDEX_SHIFT);
        Header->Name = RES_INDEX_MASK | ((i + 1) << RES_INDEX_SHIFT);
        Header->MemoryFlags = RES_DEFAULT_MEMORY_FLAGS;
        Header->LanguageId = RES_DEFAULT_LANGUAGE_ID;

        if (Icon->PngEncode) {
            Header->DataSize = Icon->PngLength;
            memcpy(&FileContent[FileOffset], (uint8_t *) Icon->PngData, Icon->PngLength);
            FileOffset += (Icon->PngLength + 3) / 4 * 4;

        } else {
            Header->DataSize = Icon->ByteCount + sizeof(dib_header);

            dib_header *DibHeader = (dib_header *) &FileContent[FileOffset];
            FileOffset += sizeof(dib_header);
            DibHeader->HeaderSize = sizeof(dib_header);
            DibHeader->Width = Icon->Size;
            DibHeader->Height = Icon->Size*2;
            DibHeader->ColorPlanes = 1;
            DibHeader->BitsPerPixel = 32;
            DibHeader->ImageSize = Icon->ByteCount;

            memcpy(&FileContent[FileOffset], (uint8_t *) Icon->Bytes, Icon->ByteCount);
            ImageSwapForWindows(&FileContent[FileOffset], Icon->Size);
            FileOffset += Icon->ByteCount;
        }
    }

    res_icon_group_a *GroupHeaderA = (res_icon_group_a *) &FileContent[FileOffset];
    FileOffset += sizeof(res_icon_group_a);
    uint16_t *GroupHeaderNameSlot = (uint16_t *) &FileContent[FileOffset];
    FileOffset += WideResourceNameSpace;
    res_icon_group_b *GroupHeaderB = (res_icon_group_b *) &FileContent[FileOffset];
    FileOffset += sizeof(res_icon_group_b);

    GroupHeaderA->DataSize =  RES_ICON_GROUP_UNCOUNTED + IconCount*sizeof(res_header_icon_group_entry);
    GroupHeaderA->HeaderSize = sizeof(res_icon_group_a) + sizeof(res_icon_group_b) + WideResourceNameSpace - RES_ICON_GROUP_UNCOUNTED;
    GroupHeaderA->Type = RES_INDEX_MASK | (RES_ICON_GROUP_TYPE << RES_INDEX_SHIFT);
    memcpy((uint8_t *) GroupHeaderNameSlot, (uint8_t *) WideResourceName, ResourceNameLength * sizeof(wchar_t));
    GroupHeaderB->MemoryFlags = RES_ICON_GROUP_MEMORY_FLAGS;
    GroupHeaderB->LanguageId = RES_DEFAULT_LANGUAGE_ID;
    GroupHeaderB->GroupType = RES_ICON_GROUP_ICON_TYPE;
    GroupHeaderB->GroupMemberCount = IconCount;

    for (int32_t i = 0; i < IconCount; i += 1) {
        icon *Icon = &Icons[i];

        res_header_icon_group_entry *Entry = (res_header_icon_group_entry *) &FileContent[FileOffset];
        FileOffset += sizeof(res_header_icon_group_entry);

        Entry->x = Icon->Size & 0xff;
        Entry->y = Icon->Size & 0xff;
        Entry->Id = i + 1;
        Entry->BitsPerPixel = 32;
        Entry->Planes = 1;
        Entry->DataBytes = Icon->PngEncode? Icon->PngLength : (Icon->ByteCount + sizeof(dib_header));
    }

    if (!write_entire_file(ResourcePath, FileContent, TotalSize)) {
        printf("Couldn't save to \"%s\"\n", ResourcePath);
        exit(1);
    }
}

void WritePng(char *Path, uint8_t *Bytes, int32_t Width, int32_t Height)
{
    int32_t PngLength;
    uint8_t *PngBytes = stbi_write_png_to_mem(Bytes, Width*4, Width, Height, 4, &PngLength);
    if (!PngBytes) {
        printf("Couldn't generate PNG for \"%s\"\n", Path);
        exit(1);
    }
    if (!write_entire_file(Path, PngBytes, PngLength)) {
        printf("Couldn't save to \"%s\"\n", Path);
        exit(1);
    }
}

int main(int arg_count, char **args)
{
    char *ico_path = 0;
    char *res_path = 0;
    char *svg_path = 0;
    
    struct { char **slot; char *ext; } inputs[] = { { &ico_path, ".ico" }, { &res_path, ".res" }, { &svg_path, ".svg" } };
    for (int i = 1; i < arg_count; ++i) {
        char *path = args[i];
        int path_length = strlen(path);
        
        for (int j = 0; j < alen(inputs); ++j) {
            int ext_length = strlen(inputs[j].ext);
            if (ext_length <= path_length && strcmp(path + path_length - ext_length, inputs[j].ext) == 0) {
                if (*(inputs[j].slot)) {
                    printf("Can't specify multiple %s files\n", inputs[j].ext);
                    return(1);
                }
                *(inputs[j].slot) = args[i];
                break;
            }
        }
    }
    
    if (!svg_path) {
        printf("You must specify at least one .svg path\n");
        return(1);
    }
    
    if (!res_path && !ico_path) {
        printf("You must specify at least one .res or .ico path\n");
        return(1);
    }
    
    NSVGimage *svg_image = nsvgParseFromFile(svg_path, "px", 72.0f);
    if (!svg_image) {
        printf("Couldn't parse \"%s\"\n", svg_path);
        return(1);
    }
    
    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();

    // NB I have no clue what heuristics windows uses to pick icons, but it has to be some of the worst code ever written. This combination of icons seems to work.
    icon icons[] = {
        { 256, 1 },
        { 48, 1 },
        { 32, 1 },
        { 16, 1 },
    };

    for (int32_t i = 0; i < alen(icons); i += 1) {
        icon *Icon = &icons[i];
        Icon->ByteCount = Icon->Size*Icon->Size * sizeof(uint32_t);
        Icon->Bytes = (uint8_t *) malloc(Icon->ByteCount);

        float Scale = ((float) Icon->Size) / max(svg_image->width, svg_image->height);
        nsvgRasterize(rasterizer, svg_image, 0, 0, Scale, Icon->Bytes, Icon->Size, Icon->Size, Icon->Size*sizeof(uint32_t));

        #if defined(DEBUG_SIZES)
        for (int32_t Pixel = 0; Pixel < Icon->Size*Icon->Size; Pixel += 1) {
            *((uint32_t *) &Icon->Bytes[Pixel*4]) = FillColors[i];
        }
        #endif
        
        #if 0
        string PathString = Format("out\\icon_test_%i.png", Icon->Size);
        path Path = PathFromString(PathString);
        WritePng(Path, Icon->Bytes, Icon->Size, Icon->Size);
        #endif

        if (Icon->PngEncode) {
            Icon->PngData = stbi_write_png_to_mem(Icon->Bytes, Icon->Size*4, Icon->Size, Icon->Size, 4, &Icon->PngLength);
            if (!Icon->PngData) {
                printf("Couldn't generate PNG for %ix%i\n", Icon->Size, Icon->Size);
                exit(1);
            }
        }
    }

    if (ico_path) {
        printf("Writing \"%s\"\n", ico_path);
        WriteIco(icons, alen(icons), ico_path);
    }

    if (res_path) {
        printf("Writing \"%s\"\n", res_path);
        WriteRes(icons, alen(icons), res_path, "WINDOW_ICON");
    }

    printf("Done! Remeber to run \"ie4uinit.exe -show\" to refresh icons in windows\n");

    return(0);
}