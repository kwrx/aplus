#include <ptk/Font.h>
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
using namespace std;
using namespace ptk;

FT_Library Font::Library;
std::unordered_map<string, FT_Face> Font::Cache;
std::unordered_map<string, string> Font::Config;

int Font::Initialize(void) {
    FILE* fp = fopen("/etc/fonts/fonts.conf", "r");
    if(!fp) {
        cerr << "ptk-warning: /etc/fonts/fonts.conf missing!" << endl;
        goto done;
    }


    char buf[BUFSIZ];
    while(fgets(buf, BUFSIZ, fp)) {
        if(!strchr(buf, ':'))
            continue;

        if(strchr(buf, '\n'))
            strchr(buf, '\n') [0] = '\0';

        
        char* p = strchr(buf, ':');
        *p++ = '\0';

        Font::Config[strdup(p)] = strdup(buf);
    }

    fclose(fp);

done:
    return FT_Init_FreeType(&Font::Library);
}

int Font::Done(void) {
    for(auto i = Font::Cache.begin(); i != Font::Cache.end(); i++)
        FT_Done_Face(i->second);

    FT_Done_FreeType(Font::Library);
    return 0;
}

string Font::GetFontPath(string family) {
    if(strchr(family.c_str(), '/'))
        return family;

    auto i = Font::Config.find(family);
    if(i != Font::Config.end())
        return i->second;

    return family;
}


Font::Font(string family, FT_F26Dot6 ptSize, FT_UInt dpi)  {
    auto p = Font::GetFontPath(family);
    auto i = Font::Cache.find(p);

    if(i == Font::Cache.end()) {
        #if 0
        int fd = open(p.c_str(), O_RDONLY);
        if(fd < 0) {
            cerr << "ptk-error: \'" << p << "\' font not found!" << endl;
            return;
        }

        struct stat st;
        fstat(fd, &st);

        void* buffer = (void*) malloc(st.st_size);
        read(fd, buffer, st.st_size);
        close(fd);

        FT_New_Memory_Face(Font::Library, (const FT_Byte*) buffer, st.st_size, 0, &this->Face);
        #endif

        if(FT_New_Face(Font::Library, p.c_str(), 0, &this->Face) != 0) {
            cerr << "ptk-error: \'" << p << "\' font not found!" << endl;
            return;
        }


        Font::Cache[p] = this->Face;
    } else
        this->Face = Font::Cache[p];

    this->PtSize = ptSize;
    this->Dpi = dpi;
}


bool Font::IsLoaded(void) {
    return !!(this->Face);
}

void Font::DrawTo(ptk::Frame* frame, string text, double x, double y) {
    this->DrawTo(frame, text, x, y, this->PtSize, this->Dpi);
}

void Font::DrawTo(ptk::Frame* frame, string text, double x, double y, FT_F26Dot6 ptSize, FT_UInt dpi) {
    //FT_Set_Char_Size(this->Face, 0, ptSize * 64, dpi, dpi);
    //FT_Set_Pixel_Sizes(this->Face, 0, 48);
    cairo_save(frame->Fx);
#if 0    
    int i;
    for(i = 0; text[i]; i++) {
        if(FT_Load_Char(this->Face, text[i], FT_LOAD_RENDER))
            continue;
        break;
        FT_GlyphSlot sl = this->Face->glyph;
            

        cairo_surface_t* mask = cairo_image_surface_create_for_data (
            (unsigned char*) sl->bitmap.buffer,
            CAIRO_FORMAT_A8,
            sl->bitmap.width,
            sl->bitmap.rows,
            sl->bitmap.width
        );

        //cairo_set_mask_rgb_color(frame->Fx, 1.0, 1.0, 1.0);
        //cairo_set_mask_alpha(Frame->Fx, 1.0);
        cairo_mask_surface(frame->Fx, mask, x + (double) sl->bitmap_left, y - (double) sl->bitmap_top);
        x += (double) (sl->advance.x >> 6);
        y += (double) (sl->advance.y >> 6);

        cairo_paint(frame->Fx);
        cairo_surface_destroy(mask);
    }
#endif

    cairo_font_face_t* face = cairo_ft_font_face_create_for_ft_face(this->Face, 0);
    if(!face) {
        fprintf(stderr, "!face!!\n");
        return;
    }
    
    //cairo_rectangle(frame->Fx, 0.0, 0.0, 1280.0, 720.0);
    cairo_set_source_rgba(frame->Fx, 1.0, 1.0, 1.0, 1.0);
    cairo_set_font_face(frame->Fx, face);
    cairo_set_font_size(frame->Fx, 12.0);
    //cairo_move_to(frame->Fx, 20, 30);
    cairo_show_text(frame->Fx, "Hello World");
    cairo_paint(frame->Fx);
    cairo_restore(frame->Fx);
}