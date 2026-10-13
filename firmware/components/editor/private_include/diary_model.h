#pragma once
#include <cstdio>
#include <cstring>

inline bool diary_leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
inline int diary_month_days(int year, int month)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return month >= 1 && month <= 12 ? days[month-1] + (month == 2 && diary_leap(year)) : 0;
}
inline bool diary_valid_date(int y, int m, int d)
{
    return y >= 2020 && y <= 2099 && d >= 1 && d <= diary_month_days(y,m);
}
inline bool diary_filename(const char *name, int *y, int *m, int *d)
{
    if (!name || strlen(name) != 13 || name[4]!='-' || name[7]!='-' || strcmp(name+10,".md")) return false;
    for (int i=0; i<10; ++i) if (i!=4 && i!=7 && (name[i]<'0' || name[i]>'9')) return false;
    return sscanf(name,"%4d-%2d-%2d.md",y,m,d)==3 && diary_valid_date(*y,*m,*d);
}
/* Monday=0; Gregorian calendar, independent of time zone and system clock. */
inline int diary_weekday(int y,int m,int d)
{
    static const int t[]={0,3,2,5,0,3,5,1,4,6,2,4};
    y-=m<3;
    return (y+y/4-y/100+y/400+t[m-1]+d+6)%7;
}
/* Original, unattributed quotations written for this diary. */
static const char *const DIARY_QUOTES[] = {
    "A small kindness can change the shape of a day.",
    "You are allowed to grow at your own pace.",
    "There is room for joy in an ordinary afternoon.",
    "Rest is part of the work of being human.",
    "Begin with the next gentle thing.",
    "Your attention is a gift you can give today.",
    "A little progress is still a reason to smile.",
    "Let a good moment last a little longer.",
    "You do not have to earn a quiet breath.",
    "There is no small act of care.",
    "Make space for the things that make you feel at home.",
    "Today can hold something you have not noticed yet.",
    "A kind word is a seed worth planting.",
    "The world is softer when we listen well.",
    "You can start again without starting from nothing.",
    "Notice what is growing, including you.",
    "A shared laugh makes a fine memory.",
    "Some days, showing up is the whole achievement.",
    "Let curiosity lead you to a small delight.",
    "Your story has room for gentle chapters.",
    "Be a place where kindness can begin.",
    "A warm drink and a slow moment can be enough.",
    "The ordinary things are often the ones we cherish.",
    "You can be proud of a quiet effort.",
    "Give yourself the patience you give a friend.",
    "A good day need not be a busy day.",
    "There is beauty in doing one thing with care.",
    "Leave a little room for wonder.",
    "What you tend with love can become a garden.",
    "The next page does not have to resemble the last.",
    "You can carry hope without having all the answers.",
    "A listening ear can be a generous shelter.",
    "Celebrate the steps nobody else can see.",
    "There is strength in asking for company.",
    "Let gratitude be specific: name one lovely thing.",
    "Your pace is not a measure of your worth.",
    "Small rituals can make a day feel like yours.",
    "A walk can be a conversation with the world.",
    "Take care of the person you are becoming.",
    "Some of the best moments arrive without a plan.",
    "You are more than the tasks you finish.",
    "Let the light through in whatever way you can.",
    "A thoughtful gesture can travel farther than you know.",
    "It is okay to choose a simpler day.",
    "Keep a little time for what delights you.",
    "An imperfect beginning can still become something lovely.",
    "Pay attention to who helps you feel like yourself.",
    "A quiet room can hold a very full life.",
    "Every season offers something worth noticing.",
    "You can make a home in small daily habits.",
    "Let yourself enjoy what is here before reaching ahead.",
    "The kindness you offer yourself matters too.",
    "A memory begins as a moment you notice.",
    "There is courage in trying with a tender heart.",
    "You may take the scenic route through a day.",
    "Good company can make the simplest meal a feast.",
    "Keep the little promises that help you feel cared for.",
    "A pause can be a wise next step.",
    "Make something small just for the pleasure of making it.",
    "Your presence can be someone's bright spot.",
    "There is room to change your mind with kindness.",
    "Let today be real before it has to be remarkable.",
    "Look for the people who make room for others.",
    "A few honest words can lighten a crowded mind.",
    "You can appreciate a moment without keeping it forever.",
    "The care you give today belongs to your story.",
    "A fresh page is an invitation, not a demand.",
    "Let yourself be a beginner at something joyful.",
    "Some treasures fit inside an ordinary conversation.",
    "Remember to include yourself in your kindness.",
    "There is a kind of wealth in having time to notice.",
    "You do not need a special occasion to be thoughtful.",
    "A little warmth is always worth sharing.",
    "Make room for laughter that has no purpose.",
    "A day can be meaningful without being impressive.",
    "The smallest welcome can make someone feel less alone.",
    "Choose one thing today that nourishes you.",
    "A gentle routine can leave room for adventure.",
    "Let your home hold signs of the life you love.",
    "You can be both a work in progress and worth caring for.",
    "Notice the effort behind another person's kindness.",
    "Some days call for courage; others call for a blanket.",
    "The good you notice does not have to cancel the hard.",
    "A moment of peace is worth making space for.",
    "You can leave a little kindness wherever you go.",
    "Let your journal hold the small things too.",
    "You are allowed to find delight in familiar places.",
    "A thoughtful question can open a lovely conversation.",
    "Care grows stronger when it is practiced gently.",
    "Give a good memory a place on the page.",
    "There is enough wonder in the sky for a short pause.",
    "Your ordinary life is worth remembering.",
    "A soft landing can be more helpful than a perfect answer.",
    "Let friendship be made of small, steady gestures.",
    "You can welcome tomorrow without rushing today.",
    "A little patience can make room for understanding.",
    "What made you smile deserves a line in your diary.",
    "Let a simple pleasure count as a real pleasure.",
    "You can finish the day with kindness toward yourself.",
    "There is still room on this page for something good."
};
static_assert(sizeof(DIARY_QUOTES)/sizeof(DIARY_QUOTES[0]) == 100, "Keep exactly 100 quotes");
