#pragma once
#include "diary_model.h"
#include "diary_store.h"
#include <functional>

static const size_t DIARY_ACTION_LIMIT=128*1024;
static inline bool diary_read_small(const std::string &path,std::string &text,bool absent=false)
{
    text.clear(); FILE *f=fopen(path.c_str(),"rb");
    if(!f) return absent && errno==ENOENT;
    char buf[1024];size_t n;bool ok=true;
    while((n=fread(buf,1,sizeof(buf),f))) {
        if(text.size()+n>DIARY_ACTION_LIMIT || memchr(buf,0,n)) {ok=false;break;}
        text.append(buf,n);
    }
    ok=ok && !ferror(f);fclose(f);return ok;
}
using diary_writer_t=std::function<bool(const std::string &,const std::string &)>;
/* The journal is persisted before touching either live file. Resume accepts
 * only the exact before/after snapshots; external changes are never replaced. */
static inline bool diary_move_resume(const std::string &root,const diary_writer_t &write)
{
    std::string base=root+"/.diary/move/",manifest;
    if(!diary_read_small(base+"pending",manifest)) return false;
    auto split=manifest.find('\n');if(split==std::string::npos) return false;
    auto from=manifest.substr(0,split),to=manifest.substr(split+1);
    int y,m,d;
    if(!diary_filename(from.c_str(),&y,&m,&d) || !diary_filename(to.c_str(),&y,&m,&d) || from==to) return false;
    std::string sb,sa,db,da,source,dest;
    if(!diary_read_small(base+"source-before",sb) || !diary_read_small(base+"source-after",sa) ||
       !diary_read_small(base+"target-before",db) || !diary_read_small(base+"target-after",da) ||
       !diary_read_small(root+"/"+from,source) || !diary_read_small(root+"/"+to,dest,true)) return false;
    if((source!=sb && source!=sa) || (dest!=db && dest!=da)) return false;
    if(dest!=da && !write(root+"/"+to,da)) return false;
    if(source!=sa && !write(root+"/"+from,sa)) return false;
    return unlink((base+"pending").c_str())==0;
}
static inline bool diary_move_prepare(const std::string &root,const std::string &from,const std::string &to,
    size_t first,size_t end,const diary_writer_t &write)
{
    int y,m,d;struct stat st;
    if(from==to || !diary_filename(from.c_str(),&y,&m,&d) || !diary_filename(to.c_str(),&y,&m,&d)) return false;
    std::string base=root+"/.diary/move/",sb,db;
    if(stat((base+"pending").c_str(),&st)==0 || errno!=ENOENT) return false;
    if(!diary_read_small(root+"/"+from,sb) || !diary_read_small(root+"/"+to,db,true) ||
       first>=end || end>sb.size() || sb.compare(first,3,"## ")) return false;
    std::string sa=sb.substr(0,first)+sb.substr(end);
    std::string da=(db.empty()?"# "+to.substr(0,10)+"\n":db)+"\n"+sb.substr(first,end-first);
    if(da.size()>DIARY_ACTION_LIMIT) return false;
    if(!diary_archive(root.c_str(),(root+"/"+from).c_str(),"versions") ||
       !diary_archive(root.c_str(),(root+"/"+to).c_str(),"versions")) return false;
    if(mkdir((root+"/.diary/move").c_str(),0700)!=0 && errno!=EEXIST) return false;
    return write(base+"source-before",sb) && write(base+"source-after",sa) &&
        write(base+"target-before",db) && write(base+"target-after",da) && write(base+"pending",from+"\n"+to);
}
