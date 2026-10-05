#include "catalog.hpp"
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <json-c/json.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace fs = std::filesystem;
namespace anto {
static std::string folded(const std::string& value) {
    gchar* text=g_utf8_casefold(value.c_str(),-1); std::string result=text ? text : ""; g_free(text); return result;
}
bool matches(const Wallpaper& wallpaper, const std::string& query, int category) {
    return (category==0 || (category==2 ? wallpaper.live : !wallpaper.live))
        && folded(wallpaper.title+" "+wallpaper.path).find(folded(query))!=std::string::npos;
}
static std::string capture(const std::vector<std::string>& args) {
    std::vector<const char*> argv; for(const auto& arg:args)argv.push_back(arg.c_str());argv.push_back(nullptr);
    GError* error=nullptr; auto* process=g_subprocess_newv(argv.data(),GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE),&error);
    if(!process) {g_clear_error(&error); return {};}
    gchar *output=nullptr,*errors=nullptr;
    bool ok=g_subprocess_communicate_utf8(process,nullptr,nullptr,&output,&errors,&error) && g_subprocess_get_successful(process);
    std::string result=ok && output ? output : "";
    g_free(output);g_free(errors);g_clear_error(&error);g_object_unref(process);return result;
}
static std::string cached_image(const Wallpaper& wallpaper,int width) {
    std::error_code error; auto stamp=fs::last_write_time(wallpaper.path,error).time_since_epoch().count();
    auto identity=wallpaper.path+":"+std::to_string(stamp)+":"+std::to_string(width);
    gchar* hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,identity.c_str(),-1);
    fs::path directory=fs::path(g_get_user_cache_dir())/"anto-desktop/wallpaper";
    fs::create_directories(directory,error);
    fs::path path=directory/(std::string(hash)+".jpg");g_free(hash);
    if(fs::is_regular_file(path,error))return path.string();
    gchar* token=g_uuid_string_random();
    fs::path temporary=path.string()+"."+token+".tmp.jpg";g_free(token);
    if(wallpaper.video) {
        capture({"timeout","8","ffmpeg","-hide_banner","-loglevel","error","-ss","1","-i",wallpaper.path,"-frames:v","1","-vf","scale="+std::to_string(width)+":-2","-y",temporary.string()});
    } else {
        GError* loadError=nullptr;
        auto* pixbuf=gdk_pixbuf_new_from_file_at_scale(wallpaper.path.c_str(),width,width,TRUE,&loadError);
        if(pixbuf) {gdk_pixbuf_save(pixbuf,temporary.c_str(),"jpeg",&loadError,"quality","88",nullptr);g_object_unref(pixbuf);}
        g_clear_error(&loadError);
    }
    if(!fs::is_regular_file(temporary,error) && !wallpaper.video)capture({"timeout","8","ffmpeg","-hide_banner","-loglevel","error","-i",wallpaper.path,"-frames:v","1","-vf","scale="+std::to_string(width)+":-2","-y",temporary.string()});
    if(fs::is_regular_file(temporary,error)) {fs::rename(temporary,path,error);if(!error)return path.string();}
    fs::remove(temporary,error);return {};
}
std::string preview_path(const Wallpaper& wallpaper) {return cached_image(wallpaper,1280);}
std::string wallpaper_directory() {
    if(const char* path=g_getenv("ANTO426_WALLPAPERS_DIR");path && *path)return path;
    fs::path config=fs::path(g_get_user_config_dir())/"anto426-local/wallpaper/gallery.json";
    json_object* settings=json_object_from_file(config.c_str());json_object* value=nullptr;
    std::string result;
    if(settings && json_object_object_get_ex(settings,"directory",&value) && json_object_is_type(value,json_type_string))result=json_object_get_string(value);
    if(settings)json_object_put(settings);
    return result.empty() ? (fs::path(g_get_home_dir())/"Pictures/Wallpapers").string() : result;
}
Catalog scan_catalog(const std::string& directory) {
    Catalog catalog;std::error_code error;
    if(fs::is_directory(directory,error)) {
        fs::recursive_directory_iterator iterator(directory,fs::directory_options::skip_permission_denied,error),end;
        while(iterator!=end && catalog.wallpapers.size()<2000) {
            const auto item=*iterator;
            if(iterator.depth()>=6)iterator.disable_recursion_pending();
            if(item.is_regular_file(error)) {
                auto extension=folded(item.path().extension().string());
                bool video=extension==".mp4" || extension==".webm" || extension==".mkv" || extension==".mov";
                bool image=extension==".png" || extension==".jpg" || extension==".jpeg" || extension==".webp" || extension==".gif" || extension==".bmp" || extension==".avif";
                if(video || image)catalog.wallpapers.push_back({item.path().string(),item.path().filename().string(),{},video || extension==".gif",video});
            }
            iterator.increment(error);if(error)break;
        }
    }
    std::sort(catalog.wallpapers.begin(),catalog.wallpapers.end(),[](const auto& left,const auto& right){return folded(left.title)<folded(right.title);});
    for(auto& item:catalog.wallpapers)item.thumbnail=cached_image(item,320);
    auto text=capture({"timeout","3","hyprctl","-j","monitors"});
    json_object* monitors=json_tokener_parse(text.c_str());
    if(monitors && json_object_is_type(monitors,json_type_array)) {
        for(size_t i=0;i<json_object_array_length(monitors);i++) {
            auto* monitor=json_object_array_get_idx(monitors,i);json_object *name=nullptr,*focused=nullptr;
            if(json_object_object_get_ex(monitor,"name",&name)) {
                json_object_object_get_ex(monitor,"focused",&focused);
                catalog.outputs.push_back({json_object_get_string(name),json_object_get_string(name),focused && json_object_get_boolean(focused)});
            }
        }
    }
    if(monitors)json_object_put(monitors);
    std::stable_sort(catalog.outputs.begin(),catalog.outputs.end(),[](const auto& left,const auto& right){
        auto rank=[](const std::string& name){return name.starts_with("eDP") ? 0 : name.starts_with("HDMI") ? 1 : name.starts_with("DP") ? 2 : 3;};
        return rank(left.name)==rank(right.name) ? left.name<right.name : rank(left.name)<rank(right.name);
    });
    fs::path states=fs::path(g_get_user_config_dir())/"anto426-local/wallpaper/outputs";
    if(fs::is_directory(states,error))for(const auto& item:fs::directory_iterator(states,error)) {
        if(item.path().extension()!=".state")continue;
        std::ifstream file(item.path());std::string output,kind,path;
        std::getline(file,output);std::getline(file,kind);std::getline(file,path);
        bool focused=std::any_of(catalog.outputs.begin(),catalog.outputs.end(),[&](const auto& target){return target.focused && target.name==output;});
        if(focused || (output=="ALL" && catalog.current.empty()))catalog.current=path;
    }
    catalog.outputs.insert(catalog.outputs.begin(),{"ALL","Tutti",false});
    catalog.outputs.push_back({"__boot_login__","Avvio e login",false});
    return catalog;
}
}
