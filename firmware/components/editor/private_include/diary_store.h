#pragma once
/* Local recovery copies live outside the flat Git working tree. No pruning:
 * a full card must fail safely rather than silently erase older writing. */
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

static inline bool diary_copy_exclusive(const char *src, const char *dst)
{
    FILE *in=fopen(src,"rb");
    if(!in) return false;
    FILE *out=fopen(dst,"wx");
    if(!out) { fclose(in); return false; }
    char buf[2048]; size_t n; bool ok=true;
    while((n=fread(buf,1,sizeof(buf),in))!=0)
        if(fwrite(buf,1,n,out)!=n) { ok=false; break; }
    if(ferror(in)) ok=false;
    if(fflush(out)!=0 || fsync(fileno(out))!=0) ok=false;
    if(fclose(out)!=0) ok=false;
    fclose(in);
    if(!ok) unlink(dst); /* Only our newly-created incomplete copy. */
    return ok;
}
static inline bool diary_archive(const char *root,const char *path,const char *kind,
                                 std::string *saved=nullptr)
{
    struct stat st;
    if(stat(path,&st)!=0) return errno==ENOENT;
    if(!S_ISREG(st.st_mode)) return false;
    const char *name=strrchr(path,'/'); name=name?name+1:path;
    std::string base=std::string(root)+"/.diary";
    if(mkdir(base.c_str(),0700)!=0 && errno!=EEXIST) return false;
    base+="/"; base+=kind;
    if(mkdir(base.c_str(),0700)!=0 && errno!=EEXIST) return false;
    /* mkdir reserves a unique slot even with an unset clock or fast saves. */
    for(unsigned i=0;i<10000;++i) {
        char stamp[48]; snprintf(stamp,sizeof(stamp),"/%010lld-%04u",(long long)time(nullptr),i);
        std::string slot=base+stamp;
        if(mkdir(slot.c_str(),0700)!=0) { if(errno==EEXIST) continue; return false; }
        std::string dest=slot+"/"+name;
        if(!diary_copy_exclusive(path,dest.c_str())) { rmdir(slot.c_str()); return false; }
        if(saved) *saved=dest;
        return true;
    }
    return false;
}
