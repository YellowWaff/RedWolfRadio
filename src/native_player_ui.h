// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
#include "rml_compat.h"
#include "native_song_popup.h"
#include "music_hotkeys.h"

namespace rwr {
struct NativePlayerSnapshot {
    std::array<wchar_t, 251> name{};
    SongPopupState state = SongPopupState::Unavailable;
    bool russian = false;
};
using NativePlayerSnapshotFn = void (*)(NativePlayerSnapshot&);
using NativePlayerQueueFn = bool (*)(MusicAction);
inline constexpr int kNativePlayerReference = 0x525752;
inline constexpr std::uint64_t kNativePlayerCookie = 0x3255495257524e50ULL;
inline constexpr size_t kNativePlayerCookieOffset = 0x234;

namespace native_player_detail {
struct Vec3 { float x, y, z; };
struct Color { float r, g, b, a; };
struct Rect { float x, y, w, h; };
struct Layout {
    float scale;
    Rect frame, title, close;
    std::array<Rect, 4> buttons;
};
inline constexpr float kFrameWidth = 360.f;
inline constexpr float kFrameHeight = 128.f;
inline constexpr float kTitleHeight = 26.f;

// Pure layout calculation, shared by drawing and the game's earlier hit pass.
inline Layout layout(float centerX, float centerY, float scale, float screenW, float screenH) {
    scale = std::max(.25f, std::min(scale, std::min((screenW - 16.f) / kFrameWidth, (screenH - 16.f) / kFrameHeight)));
    const float width = kFrameWidth * scale, height = kFrameHeight * scale;
    const float x = std::clamp(centerX - width / 2.f, 8.f, std::max(8.f, screenW - width - 8.f));
    const float y = std::clamp(centerY - height / 2.f, 8.f, std::max(8.f, screenH - height - 8.f));
    Layout result{scale, {x,y,width,height}, {x,y,width-30.f*scale,kTitleHeight*scale},
        {x+width-26.f*scale,y+3.f*scale,22.f*scale,20.f*scale}, {}};
    const float gap = 10.f * scale, bw = 44.f * scale;
    const float buttonsX = x+(width-4.f*bw-3.f*gap)/2.f;
    for (size_t i=0; i<4; ++i)
        result.buttons[i] = {buttonsX+static_cast<float>(i)*(bw+gap),y+88.f*scale,bw,28.f*scale};
    return result;
}
inline bool contains(const Rect& r, const Vec3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) &&
        p.x >= r.x && p.x < r.x+r.w && p.y >= r.y && p.y < r.y+r.h;
}

using DimensionsFn = void (*)(void*,float,float);
using PositionFn = void (*)(void*,const Vec3*);
using ColorFn = void (*)(void*,const Color*);
using ScalarFn = void (*)(void*,float);
using DrawFn = void (*)(void*,float,float,float,float,float,bool);
using MouseFn = Vec3* (*)(void*,Vec3*);
using ButtonFn = bool (*)(void*);
using CollisionFn = bool (*)(void*,Vec3*,float,float);
using PrintFn = void (*)(void*,void*,float,float,unsigned long,const wchar_t*);
using TextureFn = void (*)(void*,unsigned,void*);
struct TextRect { int left,top,right,bottom; };
using MeasureFn = TextRect* (*)(void*,TextRect*,float,float,const wchar_t*);

struct Context {
    const TsmHost* host = nullptr;
    NativePlayerSnapshotFn snapshot = nullptr;
    NativePlayerQueueFn queue = nullptr;
    DimensionsFn dimensions = nullptr;
    PositionFn position = nullptr, rotationCenter = nullptr;
    ColorFn color = nullptr;
    ScalarFn rotation = nullptr, scale = nullptr;
    DrawFn draw = nullptr;
    MouseFn mouse = nullptr;
    ButtonFn down = nullptr, press = nullptr, takePress = nullptr;
    CollisionFn collision = nullptr;
    PrintFn print = nullptr;
    // Integer identity is compared only against a live callback argument.
    // It is never converted back to a pointer or dereferenced later.
    uintptr_t dragIdentity = 0;
    float dragX = 0, dragY = 0;
    NativePlayerSnapshot lastSnapshot{};
    bool haveSnapshot = false;
    ULONGLONG openedAt = 0, lastSnapshotAt = 0;
    std::array<wchar_t,251> sourceName{};
    std::array<std::array<wchar_t,251>,2> cachedNameLines{};
    float nameWidth = 0;
    bool haveNameLines = false;
};
inline Context context;
inline unsigned char* base() { return static_cast<unsigned char*>(context.host->exeModule); }
inline bool readable(const void* p, size_t n) {
    return context.host && context.host->readablePtr && p && context.host->readablePtr(p,n);
}
template<class T> inline T read(const void* p, size_t offset) {
    T value; std::memcpy(&value,static_cast<const unsigned char*>(p)+offset,sizeof(value)); return value;
}
template<class T> inline void write(void* p, size_t offset, const T& value) {
    std::memcpy(static_cast<unsigned char*>(p)+offset,&value,sizeof(value));
}
inline void* panel() { return base()+0x9be060; }
inline void* input() { return base()+0xa54b90; }

inline bool viewport(float& width, float& height, float& scale) {
    if (!context.host || !readable(base()+0x99528c,4) || !readable(base()+0x995274,4) ||
        !readable(base()+0x992088,4)) return false;
    width = static_cast<float>(read<int>(base(),0x99528c));
    height = static_cast<float>(read<int>(base(),0x995274));
    scale = read<float>(base(),0x992088);
    return width>=320 && width<=20000 && height>=200 && height<=20000 && scale>=.25f && scale<=8.f;
}
inline bool getLayout(void* window, Layout& result) {
    float w,h,s;
    if (!viewport(w,h,s)) return false;
    float x=read<float>(window,4),y=read<float>(window,8);
    if (!std::isfinite(x)) x=w/2.f;
    if (!std::isfinite(y)) y=h/2.f;
    result=layout(x,y,s,w,h); return true;
}
inline void applyLayout(void* window,const Layout& l) {
    write(window,4,l.frame.x+l.frame.w/2.f);
    write(window,8,l.frame.y+l.frame.h/2.f);
    write(window,0x28,-l.frame.w/2.f);
    write(window,0x2c,-l.frame.h/2.f);
    // These derived dimensions are consumed by native dispatcher bookkeeping.
    write(window,0x520,l.frame.w);
    write(window,0x51c,l.frame.h);
    write(window,0x250,l.frame.y+l.frame.h);
    write(window,0x254,l.frame.y+l.frame.h);
}
inline bool guidedOverlayBlocks(void* game,void* window) {
    if(!readable(static_cast<unsigned char*>(game)+0x13bf0,sizeof(void*))) return false;
    void* overlay=read<void*>(game,0x13bf0);
    return readable(overlay,0x70) && read<unsigned char>(overlay,0x14) &&
        read<void*>(overlay,0x68)==window;
}
inline bool frontmost(void* game,void* window) {
    if (!readable(static_cast<unsigned char*>(game)+0xcac0,16)) return false;
    const auto begin=read<uintptr_t>(game,0xcac0),end=read<uintptr_t>(game,0xcac8);
    return begin && end>=begin+sizeof(void*) && end-begin<sizeof(void*)*4096 &&
        readable(reinterpret_cast<void*>(begin),sizeof(void*)) &&
        read<void*>(reinterpret_cast<void*>(begin),0)==window;
}

// Save exactly the panel properties changed by our setters. Native collision
// scratch vertices and renderer batching state remain native-owned.
struct PanelState {
    std::array<unsigned char,0x3c> properties{};
    PanelState() { std::memcpy(properties.data(),static_cast<unsigned char*>(panel())+0x288,properties.size()); }
    ~PanelState() { std::memcpy(static_cast<unsigned char*>(panel())+0x288,properties.data(),properties.size()); }
};
inline void geometry(const Rect& r) {
    const Vec3 pos{r.x+r.w/2.f,r.y+r.h/2.f,0.f},zero{};
    context.dimensions(panel(),r.w,r.h); context.position(panel(),&pos);
    context.rotation(panel(),0.f); context.rotationCenter(panel(),&zero); context.scale(panel(),1.f);
}
inline bool hit(const Rect& r,const Vec3& mouse) {
    // Native window hit tests compare screen coordinates directly. Shared
    // panel Collision also applies the last renderer's clipping state.
    return contains(r,mouse);
}
inline bool whiteTexture(void* game) {
    if (!readable(static_cast<unsigned char*>(game)+0xb048,sizeof(void*)) ||
        !readable(base()+0x9ead08,sizeof(void*))) return false;
    auto* texture=read<void*>(game,0xb048);
    if (!readable(texture,sizeof(void*))) return false;
    auto* table=read<void*>(texture,0);
    if (!readable(table,0x78)) return false;
    const auto bind=read<TextureFn>(table,0x70);
    void* shader=read<void*>(base(),0x9ead08);
    if (!readable(reinterpret_cast<void*>(bind),1) || !readable(shader,sizeof(void*))) return false;
    bind(texture,0,shader); return true;
}
inline void rectangle(void* game,const Rect& r, const Color& color) {
    // Printing may bind the font atlas between rectangles. Bind the game's
    // white texture for every flat panel, not only at the start of the frame.
    if(!whiteTexture(game)) return;
    geometry(r); context.color(panel(),&color); context.draw(panel(),0.f,0.f,1.f,1.f,0.f,true);
}
// Build transport symbols from the already verified flat-panel API. A fixed
// row count bounds native draws even at large UI scales and avoids font-glyph
// or texture dependencies. The complete button remains the input target.
inline void triangle(void* game,const Rect& bounds,bool right,const Color& color) {
    constexpr int rows=16;
    for(int row=0;row<rows;++row) {
        const float t=(static_cast<float>(row)+.5f)/static_cast<float>(rows);
        const float width=bounds.w*(1.f-std::abs(2.f*t-1.f));
        rectangle(game,{right?bounds.x:bounds.x+bounds.w-width,
            bounds.y+static_cast<float>(row)*bounds.h/static_cast<float>(rows),
            width,bounds.h/static_cast<float>(rows)},color);
    }
}
inline void transportIcon(void* game,const Rect& button,float scale,size_t index,bool showPlay) {
    const float x=button.x+button.w/2.f,y=button.y+button.h/2.f;
    constexpr Color ink{.95f,.95f,.95f,1.f};
    const auto bar=[&](float dx,float dy,float width,float height) {
        rectangle(game,{x+dx*scale,y+dy*scale,width*scale,height*scale},ink);
    };
    const auto arrow=[&](float dx,float dy,float width,float height,bool right) {
        triangle(game,{x+dx*scale,y+dy*scale,width*scale,height*scale},right,ink);
    };
    switch(index) {
    case 0: // Previous: left arrow against a beginning bar.
        bar(-8.f,-8.f,3.f,16.f); arrow(-3.f,-7.f,10.f,14.f,false); break;
    case 1: // Restart: bent return arrow, distinct from Previous.
        bar(-6.f,-7.f,12.f,3.f); bar(3.f,-7.f,3.f,13.f);
        bar(-4.f,3.f,10.f,3.f); arrow(-9.f,0.f,6.f,9.f,false); break;
    case 2:
        if(showPlay) arrow(-6.f,-8.f,12.f,16.f,true);
        else { bar(-6.f,-8.f,4.f,16.f); bar(2.f,-8.f,4.f,16.f); }
        break;
    case 3: // Next: right arrow against an end bar.
        arrow(-7.f,-7.f,10.f,14.f,true); bar(5.f,-8.f,3.f,16.f); break;
    default: break;
    }
}
inline void text(float x,float y,const wchar_t* value,unsigned long color=0xfff0f0f0u) {
    auto* font=read<void*>(base(),0x994200);
    context.print(base()+0x996fb0,font,x,y,color,value);
}
inline float textWidth(const wchar_t* value) {
    void* font=read<void*>(base(),0x994200);
    if(!readable(font,sizeof(void*))) return -1.f;
    void* table=read<void*>(font,0);
    if(!readable(table,0x58)) return -1.f;
    const auto measure=read<MeasureFn>(table,0x50);
    if(!readable(reinterpret_cast<void*>(measure),1)) return -1.f;
    TextRect bounds{};
    measure(font,&bounds,0.f,0.f,value);
    const auto width=static_cast<long long>(bounds.right)-bounds.left;
    return width>=0 && width<100000 ? static_cast<float>(width) : -1.f;
}
inline const wchar_t* stateText(const NativePlayerSnapshot& s) {
    switch(s.state) {
    case SongPopupState::Playing:return s.russian?L"Сейчас играет":L"Now playing";
    case SongPopupState::Paused:return s.russian?L"Пауза":L"Paused";
    case SongPopupState::Loading:return s.russian?L"Загрузка":L"Loading";
    case SongPopupState::Stopped:return s.russian?L"Сейчас ничего не играет":L"No track is playing";
    case SongPopupState::Busy:return s.russian?L"Обновление…":L"Updating…";
    default:return s.russian?L"Информация недоступна":L"Song information unavailable";
    }
}
inline std::array<std::array<wchar_t,251>,2> nameLines(std::wstring_view name,size_t columns) {
    std::array<std::array<wchar_t,251>,2> lines{};
    columns=std::clamp(columns,size_t(2),size_t(249));
    size_t source=0;
    for(size_t line=0;line<2 && source<name.size();++line) {
        size_t used=0;
        while(source<name.size() && used<columns) {
            wchar_t c=name[source];
            if(c>=0xd800 && c<=0xdbff && source+1<name.size() && name[source+1]>=0xdc00 && name[source+1]<=0xdfff) {
                if(used+2>columns) break;
                lines[line][used++]=c; lines[line][used++]=name[++source]; ++source;
            } else {
                if(c<32 || (c>=127 && c<=159)) c=L' ';
                if(c>=0xd800 && c<=0xdfff) c=L'\ufffd';
                lines[line][used++]=c; ++source;
            }
        }
        if(line==1 && source<name.size()) {
            if(used && lines[line][used-1]>=0xdc00 && lines[line][used-1]<=0xdfff) --used;
            if(used) --used;
            lines[line][used++]=L'\u2026'; lines[line][used]=0;
        }
    }
    return lines;
}
inline std::array<std::array<wchar_t,251>,2> measuredNameLines(std::wstring_view name,float width,size_t fallbackColumns) {
    if(textWidth(L"M")<=0.f) return nameLines(name,fallbackColumns);
    // Normalize control characters/surrogates without treating '%' specially.
    std::array<wchar_t,251> clean{};
    size_t n=0;
    for(size_t i=0;i<name.size() && n<250;++i) {
        wchar_t c=name[i];
        if(c>=0xd800 && c<=0xdbff && i+1<name.size() && name[i+1]>=0xdc00 && name[i+1]<=0xdfff) {
            if(n+2>250) break;
            clean[n++]=c; clean[n++]=name[++i];
        } else {
            if(c<32 || (c>=127 && c<=159)) c=L' ';
            if(c>=0xd800 && c<=0xdfff) c=L'\ufffd';
            clean[n++]=c;
        }
    }
    std::array<std::array<wchar_t,251>,2> lines{};
    size_t source=0;
    for(size_t line=0;line<2 && source<n;++line) {
        size_t used=0;
        while(source<n) {
            const size_t chars=(clean[source]>=0xd800 && clean[source]<=0xdbff && source+1<n)?2:1;
            for(size_t k=0;k<chars;++k) lines[line][used+k]=clean[source+k];
            lines[line][used+chars]=0;
            if(textWidth(lines[line].data())>width) {
                lines[line][used]=0; break;
            }
            used+=chars; source+=chars;
        }
        if(line==1 && source<n) {
            while(used) {
                if(lines[line][used-1]>=0xdc00 && lines[line][used-1]<=0xdfff) --used;
                if(used) --used;
                lines[line][used]=L'\u2026'; lines[line][used+1]=0;
                if(textWidth(lines[line].data())<=width) break;
            }
        }
    }
    return lines;
}
} // namespace native_player_detail

inline bool initializeNativePlayerUi(const TsmHost* host,NativePlayerSnapshotFn snapshot,NativePlayerQueueFn queue) {
    using namespace native_player_detail;
    if(!host || !host->readablePtr || !host->exeModule || !host->engineModule || !snapshot || !queue ||
        host->exeSize<0xa54b90+0x700) return false;
    Context candidate{}; candidate.host=host; candidate.snapshot=snapshot; candidate.queue=queue;
    const auto engine=static_cast<HMODULE>(host->engineModule);
#define RWR_NATIVE_EXPORT(member,type,name) candidate.member=reinterpret_cast<type>(GetProcAddress(engine,name)); if(!candidate.member) return false
    RWR_NATIVE_EXPORT(dimensions,DimensionsFn,"?SetDimensions@C3D_PANEL2D@@QEAAXMM@Z");
    RWR_NATIVE_EXPORT(position,PositionFn,"?SetPosition@C3D_PANEL2D@@QEAAXAEBVC3DVECTOR3@@@Z");
    RWR_NATIVE_EXPORT(rotationCenter,PositionFn,"?SetRotationCentre@C3D_PANEL2D@@QEAAXAEBVC3DVECTOR3@@@Z");
    RWR_NATIVE_EXPORT(color,ColorFn,"?SetColor@C3D_PANEL2D@@QEAAXAEBVC3DFCOLOR@@@Z");
    RWR_NATIVE_EXPORT(rotation,ScalarFn,"?SetRotation@C3D_PANEL2D@@QEAAXM@Z");
    RWR_NATIVE_EXPORT(scale,ScalarFn,"?SetScale@C3D_PANEL2D@@QEAAXM@Z");
    RWR_NATIVE_EXPORT(draw,DrawFn,"?Draw@C3D_PANEL2D@@QEAAXMMMMM_N@Z");
    RWR_NATIVE_EXPORT(mouse,MouseFn,"?GetMouseSolid@C3D_INPUT@@QEAA?AVC3DVECTOR3@@XZ");
    RWR_NATIVE_EXPORT(down,ButtonFn,"?GetMouseLeftDown@C3D_INPUT@@QEAA_NXZ");
    RWR_NATIVE_EXPORT(press,ButtonFn,"?GetMouseLeftPress@C3D_INPUT@@QEAA_NXZ");
    RWR_NATIVE_EXPORT(takePress,ButtonFn,"?GetMouseLeftPressAOff@C3D_INPUT@@QEAA_NXZ");
    RWR_NATIVE_EXPORT(collision,CollisionFn,"?Collision@C3D_PANEL2D@@QEAA_NVC3DVECTOR3@@MM@Z");
    RWR_NATIVE_EXPORT(print,PrintFn,"?PrintLeftUnicodeNoArg@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_W@Z");
#undef RWR_NATIVE_EXPORT
    auto* b=static_cast<unsigned char*>(host->exeModule);
    if(!host->readablePtr(b+0x9be060,0x6750) || !host->readablePtr(b+0xa54b90,0x700) ||
        !host->readablePtr(b+0x994200,sizeof(void*)) || !host->readablePtr(b+0x996fb0,sizeof(void*))) return false;
    context=candidate; return true;
}

inline bool isNativePlayerWindow(void* window) {
    using namespace native_player_detail;
    return readable(window,0x250) && read<int>(window,0x23c)==kNativePlayerReference &&
        read<int>(window,0x34)==1 && read<int>(window,0x38)==0x983e &&
        read<int>(window,0x248)==4 && read<int>(window,0x24c)==1 &&
        read<std::uint64_t>(window,kNativePlayerCookieOffset)==kNativePlayerCookie;
}
inline bool markNativePlayerWindow(void* window) {
    using namespace native_player_detail;
    if(!readable(window,0x5d8) || read<int>(window,0x23c)!=kNativePlayerReference ||
        read<int>(window,0x34)!=1 || read<int>(window,0x38)!=0x983e ||
        read<int>(window,0x248)!=4 || read<int>(window,0x24c)!=1) return false;
    write(window,kNativePlayerCookieOffset,kNativePlayerCookie);
    context.dragIdentity=0; context.haveSnapshot=false; context.openedAt=GetTickCount64();
    context.lastSnapshotAt=0; context.haveNameLines=false;
    Layout l{}; if(getLayout(window,l)) applyLayout(window,l);
    return true;
}

inline bool nativePlayerHit(void* game,void* window) {
    using namespace native_player_detail;
    (void)game;
    if(!isNativePlayerWindow(window) || read<unsigned char>(window,0)!=0) return false;
    Layout l{}; if(!getLayout(window,l)) return false;
    if(context.dragIdentity==reinterpret_cast<uintptr_t>(window) && context.down(input())) return true;
    Vec3 mouse{}; context.mouse(input(),&mouse);
    if(read<unsigned char>(window,1)) l.frame.h=kTitleHeight*l.scale;
    return hit(l.frame,mouse);
}

inline void drawNativePlayerFrame(void* game,void* window,int index) {
    using namespace native_player_detail;
    (void)index;
    if(!isNativePlayerWindow(window) || !readable(window,0x5d8) || read<unsigned char>(window,0)!=0 ||
        !readable(game,0xcb88)) return;
    Layout l{}; if(!getLayout(window,l)) return;
    auto* font=read<void*>(base(),0x994200);
    if(!readable(font,sizeof(void*))) return;
    Vec3 mouse{}; context.mouse(input(),&mouse);
    const bool front=frontmost(game,window);
    const bool down=context.down(input());
    const auto now=GetTickCount64();
    const bool inputAllowed=front && now-context.openedAt>=350 &&
        !read<unsigned char>(game,0xfc1) && read<int>(game,0xad74)<0 &&
        !guidedOverlayBlocks(game,window);
    if(context.dragIdentity==reinterpret_cast<uintptr_t>(window)) {
        if(!down || !inputAllowed) context.dragIdentity=0;
        else {
            float w,h,s;
            if(viewport(w,h,s)) l=layout(mouse.x-context.dragX,mouse.y-context.dragY,s,w,h);
        }
    }
    applyLayout(window,l);
    // Retain native window-manager hover/focus bookkeeping, while handling this
    // small frame's drawing and input ourselves.
    write(window,0x18,.6f); write(window,0x1c,1.1f);
    write<unsigned char>(window,0x20,inputAllowed?1:0);
    PanelState restore;
    Rect visibleFrame=l.frame;
    const bool minimized=read<unsigned char>(window,1)!=0;
    if(minimized) visibleFrame.h=kTitleHeight*l.scale;
    const bool hovered=hit(visibleFrame,mouse) || context.dragIdentity==reinterpret_cast<uintptr_t>(window);
    write<unsigned char>(window,0x10,hovered?1:0);
    if(hovered) { write<unsigned char>(game,0xfc3,1); write(game,0xcb80,window); }
    // A click on an exposed background window remains available to the native
    // dispatcher's end-of-frame focus promotion. Only foreground actions consume.
    if(inputAllowed && hovered && context.press(input())) {
        if(hit(l.close,mouse) && context.takePress(input())) {
            write<unsigned char>(window,0,1); write(game,0xfc8,-.5f);
            context.dragIdentity=0; return;
        }
        if(hit(l.title,mouse) && context.takePress(input())) {
            context.dragIdentity=reinterpret_cast<uintptr_t>(window);
            context.dragX=mouse.x-(l.frame.x+l.frame.w/2.f);
            context.dragY=mouse.y-(l.frame.y+l.frame.h/2.f);
        }
    }
    // The snapshot can inspect a large music catalog. A tenth of a second is
    // sufficient for this window and keeps UI rendering independent of its size.
    if(!context.haveSnapshot || now-context.lastSnapshotAt>=100) {
        NativePlayerSnapshot next{}; context.snapshot(next); next.name.back()=0;
        if(next.state!=SongPopupState::Busy || !context.haveSnapshot) {
            context.lastSnapshot=next; context.haveSnapshot=true;
        }
        context.lastSnapshotAt=now;
    }
    const auto& snapshot=context.lastSnapshot;
    const bool showPlay=snapshot.state!=SongPopupState::Playing && snapshot.state!=SongPopupState::Loading;
    const wchar_t* labels[4] = {snapshot.russian?L"Предыдущая":L"Previous",
        snapshot.russian?L"Сначала":L"Restart",
        showPlay?(snapshot.russian?L"Играть":L"Play"):(snapshot.russian?L"Пауза":L"Pause"),
        snapshot.russian?L"Следующая":L"Next"};
    const wchar_t* caption=L"Red Wolf Radio";
    if(front && !minimized)
        for(size_t i=0;i<4;++i) if(hit(l.buttons[i],mouse)) { caption=labels[i]; break; }
    if(!whiteTexture(game)) return;
    rectangle(game,visibleFrame,{.095f,.105f,.12f,1.f});
    rectangle(game,{l.frame.x,l.frame.y,l.frame.w,kTitleHeight*l.scale},{.31f,.085f,.075f,1.f});
    rectangle(game,l.close,hit(l.close,mouse)&&front?Color{.6f,.17f,.13f,1.f}:Color{.4f,.12f,.1f,1.f});
    text(l.frame.x+12.f*l.scale,l.frame.y+4.f*l.scale,caption);
    text(l.close.x+6.f*l.scale,l.close.y+1.f*l.scale,L"X");
    if(minimized) return;
    std::array<wchar_t,251> displayedName=snapshot.name;
    if(!displayedName[0]) {
        const std::wstring_view fallback=stateText(snapshot);
        std::copy_n(fallback.begin(),std::min(fallback.size(),displayedName.size()-1),displayedName.begin());
    }
    const float nameWidth=l.frame.w-32.f*l.scale;
    if(!context.haveNameLines || context.sourceName!=displayedName || context.nameWidth!=nameWidth) {
        context.cachedNameLines=measuredNameLines(displayedName.data(),nameWidth,
            static_cast<size_t>(nameWidth/(15.f*l.scale)));
        context.sourceName=displayedName; context.nameWidth=nameWidth; context.haveNameLines=true;
    }
    const auto& lines=context.cachedNameLines;
    text(l.frame.x+16.f*l.scale,l.frame.y+34.f*l.scale,lines[0].data());
    text(l.frame.x+16.f*l.scale,l.frame.y+56.f*l.scale,lines[1].data());
    constexpr MusicAction actions[4]={MusicAction::PreviousTrack,MusicAction::Restart,MusicAction::PlayPause,MusicAction::Next};
    for(size_t i=0;i<4;++i) {
        const bool hot=front && hit(l.buttons[i],mouse);
        rectangle(game,l.buttons[i],hot?Color{.36f,.38f,.42f,1.f}:Color{.22f,.24f,.28f,1.f});
        transportIcon(game,l.buttons[i],l.scale,i,showPlay);
        if(inputAllowed && hot && context.takePress(input())) {
            if(context.queue(actions[i])) context.lastSnapshotAt=0;
        }
    }
    if(inputAllowed && hovered) context.takePress(input());
}
} // namespace rwr
