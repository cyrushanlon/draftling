#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/* Plain Markdown, no database or private markers. Only the requested page is
 * retained, even when a day contains thousands of sections or a huge line. */
struct diary_section_t {
    long start=0, end=0;
    std::string title="Earlier writing", excerpt;
};
static inline std::string diary_trim(std::string s)
{
    auto a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return "";
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
static inline std::string diary_short_text(std::string s,size_t max)
{
    if(s.size()>max) {
        size_t n=max;
        while(n && ((unsigned char)s[n]&0xc0)==0x80) --n;
        s.resize(n);
    }
    return s;
}
static inline bool diary_timeline(FILE *f,size_t first,size_t limit,
                                  std::vector<diary_section_t> &page,size_t &total)
{
    page.clear(); total=0;
    diary_section_t section;
    bool content=false; char fence=0; size_t fence_len=0;
    auto emit=[&](long end) {
        if(!content) return;
        section.end=end;
        if(total>=first && page.size()<limit) page.push_back(section);
        ++total;
    };
    char buf[257];
    while(true) {
        long offset=ftell(f);
        if(offset<0) return false;
        if(!fgets(buf,sizeof(buf),f)) break;
        std::string line=buf;
        /* Drain the rest of a long line, without treating a continuation as
         * another heading. Keep the excerpt's UTF-8 tail valid. */
        if(line.back()!='\n') { int c; while((c=fgetc(f))!=EOF && c!='\n') {} }
        line=diary_short_text(line,250);
        if(offset==0 && line.compare(0,3,"\xef\xbb\xbf")==0) line.erase(0,3);
        auto trimmed=diary_trim(line);
        if(line.rfind("<!-- diary: bookmark=",0)==0) continue;
        size_t indent=line.find_first_not_of(' ');
        if(indent!=std::string::npos && indent<=3 && (line[indent]=='`' || line[indent]=='~')) {
            char ch=line[indent]; size_t n=0; while(indent+n<line.size() && line[indent+n]==ch) ++n;
            if(n>=3) {
                if(!fence) { fence=ch; fence_len=n; }
                else if(ch==fence && n>=fence_len && diary_trim(line.substr(indent+n)).empty()) fence=0;
                content=true; continue;
            }
        }
        if(!fence && line.rfind("## ",0)==0) {
            emit(offset); section=diary_section_t{};
            section.start=offset; section.title=diary_trim(line.substr(3));
            if(section.title.empty()) section.title="Moment";
            content=true;
        } else if(!trimmed.empty() && !(!content && line.rfind("# ",0)==0)) {
            content=true;
            if(section.excerpt.empty()) section.excerpt=diary_short_text(trimmed,100);
        }
    }
    long end=ftell(f);
    if(ferror(f) || end<0) return false;
    emit(end); return true;
}
static const char *const DIARY_MOODS[]={"Good","Okay","Low","Mixed"};
static const char *const DIARY_ENERGIES[]={"Low","Steady","High"};
static inline std::string diary_moment_markdown(const std::string &stamp,
    const std::string &title,const std::string &body,int mood=-1,int energy=-1)
{
    std::string text=diary_trim(body), name=diary_trim(title);
    bool check=mood>=0 && mood<4, power=energy>=0 && energy<3;
    if(text.empty() && !check && !power) return "";
    /* A title is one Markdown line, even when pasted from another editor. */
    for(char &c:name) if(c=='\r' || c=='\n') c=' ';
    if(name.empty()) name=(check || power)?"Check-in":"Moment";
    std::string out="\n\n## "+(stamp.empty()?"":stamp+" - ")+name+"\n\n";
    if(check) out+=std::string("Feeling: ")+DIARY_MOODS[mood]+"\n";
    if(power) out+=std::string("Energy: ")+DIARY_ENERGIES[energy]+"\n";
    if(!text.empty()) out+=(check || power?"\n":"")+text+"\n";
    return out;
}
