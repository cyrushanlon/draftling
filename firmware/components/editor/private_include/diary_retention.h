#pragma once
#include "diary_store.h"
#include <dirent.h>
#include <vector>
#include <map>
#include <algorithm>

static inline int diary_copy_stat(const char *path,struct stat *st)
{
#ifdef ESP_PLATFORM
    /* The SD card uses FAT, which has no symbolic links. */
    return stat(path,st);
#else
    return lstat(path,st);
#endif
}

struct diary_copy_t { std::string path,name,slot; size_t bytes=0; };
static inline bool diary_slot_name(const char *s)
{
    if(strlen(s)!=15 || s[10]!='-') return false;
    for(int i=0;i<15;++i) if(i!=10 && (s[i]<'0' || s[i]>'9')) return false;
    return true;
}
/* Only the archive writer's exact two-level layout, never recursive deletion. */
static inline bool diary_copies(const std::string &root,const char *kind,std::vector<diary_copy_t> &out)
{
    out.clear(); std::string base=root+"/.diary/"+kind;
    struct stat parent;
    if(diary_copy_stat((root+"/.diary").c_str(),&parent)!=0) return errno==ENOENT;
    if(!S_ISDIR(parent.st_mode)) return false;
    if(diary_copy_stat(base.c_str(),&parent)!=0) return errno==ENOENT;
    if(!S_ISDIR(parent.st_mode)) return false;
    DIR *d=opendir(base.c_str());if(!d) return errno==ENOENT;
    bool ok=true;
    while(auto ent=readdir(d)) {
        if(!diary_slot_name(ent->d_name)) continue;
        std::string slot=base+"/"+ent->d_name;struct stat st;
        if(diary_copy_stat(slot.c_str(),&st)!=0) {ok=false;break;}
        if(!S_ISDIR(st.st_mode)) continue;
        DIR *sub=opendir(slot.c_str());if(!sub) {ok=false;break;}
        while(auto file=readdir(sub)) {
            if(file->d_name[0]=='.' || strchr(file->d_name,'/')) continue;
            std::string p=slot+"/"+file->d_name;
            if(diary_copy_stat(p.c_str(),&st)!=0) {ok=false;break;}
            if(S_ISREG(st.st_mode)) {
                if(out.size()>=10000) {ok=false;break;}
                out.push_back({p,file->d_name,ent->d_name,(size_t)st.st_size});
            }
        }
        closedir(sub);if(!ok) break;
    }
    closedir(d);
    std::sort(out.begin(),out.end(),[](const diary_copy_t &a,const diary_copy_t &b){return a.slot>b.slot;});
    return ok;
}
static inline std::vector<diary_copy_t> diary_prune_plan(const std::vector<diary_copy_t> &copies,unsigned keep)
{
    std::vector<diary_copy_t> out; if(!keep) return out;
    std::map<std::string,unsigned> count;
    for(const auto &copy:copies) if(++count[copy.name]>keep) out.push_back(copy);
    return out;
}
static inline bool diary_prune_versions(const std::string &root,unsigned keep,size_t &removed)
{
    removed=0;if(keep<20) return false;
    std::vector<diary_copy_t> files;if(!diary_copies(root,"versions",files)) return false;
    for(const auto &copy:diary_prune_plan(files,keep)) {
        struct stat st;
        if(diary_copy_stat(copy.path.c_str(),&st)!=0 || !S_ISREG(st.st_mode) || (size_t)st.st_size!=copy.bytes) return false;
        if(unlink(copy.path.c_str())!=0) return false;
        ++removed;auto slash=copy.path.find_last_of('/');rmdir(copy.path.substr(0,slash).c_str());
    }
    return true;
}
