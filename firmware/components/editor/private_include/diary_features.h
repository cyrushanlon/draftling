#pragma once
#include "diary_moments.h"
#include <algorithm>

static const char *const DIARY_TOPICS[]={"Family","Friends","Travel","Ideas","Work","Nature","Gratitude","Learning"};
struct diary_labels_t { bool star=false; unsigned tags=0; };
static inline diary_labels_t diary_labels(const std::string &text)
{
    diary_labels_t out;
    size_t a=text.find("<!-- diary: bookmark=");
    if(a==std::string::npos) return out;
    size_t b=text.find(" -->",a); if(b==std::string::npos) return out;
    auto line=text.substr(a,b-a); out.star=line.find("bookmark=1")!=std::string::npos;
    auto t=line.find(" tags="); if(t==std::string::npos) return out;
    auto tags=","+line.substr(t+6)+",";
    for(int i=0;i<8;++i) if(tags.find(","+std::string(DIARY_TOPICS[i])+",")!=std::string::npos) out.tags|=1u<<i;
    return out;
}
static inline std::string diary_with_labels(std::string text,diary_labels_t labels)
{
    size_t a=0;
    while((a=text.find("<!-- diary: bookmark=",a))!=std::string::npos) {
        auto b=text.find(" -->",a); if(b==std::string::npos) break;
        b+=4;if(b<text.size() && text[b]=='\n') ++b;
        text.erase(a,b-a);
    }
    std::string tags;
    for(int i=0;i<8;++i) if(labels.tags&(1u<<i)) { if(!tags.empty()) tags+=","; tags+=DIARY_TOPICS[i]; }
    size_t at=text.rfind("# ",0)==0?text.find('\n')+1:0;
    text.insert(at,"<!-- diary: bookmark="+std::string(labels.star?"1":"0")+" tags="+tags+" -->\n");
    return text;
}
/* Accept ordinary two-sided Git conflict blocks only. Refuse malformed or
 * diff3 blocks rather than dropping text we do not understand. */
static inline bool diary_resolve(const std::string &text,int choice,std::string &out,int &blocks)
{
    out.clear(); blocks=0; int state=0;
    for(size_t p=0;p<text.size();) {
        auto end=text.find('\n',p);end=end==std::string::npos?text.size():end+1;
        auto line=text.substr(p,end-p);p=end;
        if(line.rfind("<<<<<<<",0)==0) { if(state) return false; state=1; ++blocks; }
        else if(line.rfind("=======",0)==0) { if(state!=1 || diary_trim(line)!="=======") return false; state=2; }
        else if(line.rfind(">>>>>>>",0)==0) { if(state!=2) return false; state=0; }
        else if(line.rfind("|||||||",0)==0) return false;
        else if(!state || choice==0 || (choice==1 && state==1) || (choice==2 && state==2)) out+=line;
    }
    return state==0 && blocks>0;
}
static inline std::string diary_without_private_markers(const std::string &text)
{
    std::string out;
    for(size_t p=0;p<text.size();) {
        auto end=text.find('\n',p);end=end==std::string::npos?text.size():end+1;
        auto line=text.substr(p,end-p);p=end;
        if(line.rfind("<!-- diary: bookmark=",0)!=0) out+=line;
    }
    return out;
}
/* Rules are explicit POSIX TZ presets, not a bundled worldwide tzdb. */
static const char *const DIARY_ZONE_NAMES[]={"Manual offset","UTC","UK / Ireland","Central Europe","US Eastern","US Central","US Mountain","US Pacific","Sydney"};
static const char *const DIARY_ZONE_RULES[]={"","UTC0","GMT0BST,M3.5.0/1,M10.5.0/2","CET-1CEST,M3.5.0/2,M10.5.0/3","EST5EDT,M3.2.0/2,M11.1.0/2","CST6CDT,M3.2.0/2,M11.1.0/2","MST7MDT,M3.2.0/2,M11.1.0/2","PST8PDT,M3.2.0/2,M11.1.0/2","AEST-10AEDT,M10.1.0/2,M4.1.0/3"};
