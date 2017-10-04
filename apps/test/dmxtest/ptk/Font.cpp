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


cairo_font_face_t* Font::Load(string family, int flags)  {
    FT_Face Face;
    auto p = Font::GetFontPath(family);
    auto i = Font::Cache.find(p);

    if(i == Font::Cache.end()) {
        if(FT_New_Face(Font::Library, p.c_str(), 0, &Face) != 0) {
            cerr << "ptk-error: \'" << p << "\' font not found!" << endl;
            return NULL;
        }


        Font::Cache[p] = Face;
    } else
        Face = Font::Cache[p];

    cairo_font_face_t* ft = cairo_ft_font_face_create_for_ft_face(Face, flags);
    if(!ft)
        return NULL;

    return ft;
}
