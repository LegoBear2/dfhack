//command-prompt a one line command entry at the top of the screen for quick commands

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <ColorText.h>

#include <modules/Screen.h>
#include <modules/Gui.h>

#include <set>
#include <list>
#include <utility>

#include "df/interface_key.h"
#include "df/ui.h"
#include "df/graphic.h"
#include "df/enabler.h"

using namespace DFHack;
using namespace df::enums;

using df::global::ui;
using df::global::gps;
using df::global::enabler;

class viewscreen_commandpromptst;
class prompt_ostream:public buffered_color_ostream
{
    viewscreen_commandpromptst *parent_;
    protected:
        void flush_proxy();
    public:
        prompt_ostream(viewscreen_commandpromptst* parent):parent_(parent){}
        bool empty(){return buffer.empty();}
};
class viewscreen_commandpromptst : public dfhack_viewscreen {
public:
    void feed(std::set<df::interface_key> *events);

    void logic() {
        dfhack_viewscreen::logic();
    }

    void render();
    void help() { }

    std::string getFocusString() { return "commandprompt"; }
    viewscreen_commandpromptst(std::string entry):is_response(false),entry(entry)
    {
        show_fps=df::global::gps->display_frames;
        df::global::gps->display_frames=0;
        cursor_pos = 0;
    }
    ~viewscreen_commandpromptst()
    {
        df::global::gps->display_frames=show_fps;
    }

    void add_response(color_value v, std::string s)
    {
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part))
        {
            responses.push_back(std::make_pair(v, part + '\n'));
        }
    }
protected:
    std::list<std::pair<color_value,std::string> > responses;
    int cursor_pos;
    bool is_response;
    bool show_fps;
    int frame;
    void submit();
    std::string entry;
};
void prompt_ostream::flush_proxy()
{
    if (buffer.empty())
        return;
    for(auto it=buffer.begin();it!=buffer.end();it++)
        parent_->add_response(it->first,it->second);
    buffer.clear();
}
void viewscreen_commandpromptst::render()
{
    ++frame;
    if (frame >= df::global::enabler->gfps)
        frame = 0;
    if (Screen::isDismissed(this))
        return;

    dfhack_viewscreen::render();

    auto dim = Screen::getWindowSize();
    parent->render();
    if(is_response)
    {
        auto it=responses.begin();
        for(int i=0;i<dim.y && it!=responses.end();i++,it++)
        {
            Screen::fillRect(Screen::Pen(' ', 7, 0),0,i,dim.x,i);
            std::string cur_line=it->second;
            Screen::paintString(Screen::Pen(' ',it->first,0),0,i,cur_line.substr(0,cur_line.size()-1));
        }
    }
    else
    {
        Screen::fillRect(Screen::Pen(' ', 7, 0),0,0,dim.x,0);
        Screen::paintString(Screen::Pen(' ', 7, 0), 0, 0,"[DFHack]#");
        std::string cursor = (frame < df::global::enabler->gfps / 2) ? "_" : " ";
        if(cursor_pos < (dim.x - 10))
        {
            Screen::paintString(Screen::Pen(' ', 7, 0), 10,0 , entry);
            if (entry.size() > dim.x - 10)
                Screen::paintTile(Screen::Pen('\032', 7, 0), dim.x - 1, 0);
            if (cursor != " ")
                Screen::paintString(Screen::Pen(' ', 10, 0), 10 + cursor_pos, 0, cursor);
        }
        else
        {
            size_t start = cursor_pos - dim.x + 10 + 1;
            Screen::paintTile(Screen::Pen('\033', 7, 0), 9, 0);
            Screen::paintString(Screen::Pen(' ', 7, 0), 10, 0, entry.substr(start));
            if (cursor != " ")
                Screen::paintString(Screen::Pen(' ', 10, 0), dim.x - 1, 0, cursor);
        }
    }
}
void viewscreen_commandpromptst::submit()
{
    CoreSuspendClaimer suspend;
    if(is_response)
    {
        Screen::dismiss(this);
        return;
    }
    //color_ostream_proxy out(Core::getInstance().getConsole());
    prompt_ostream out(this);
    Core::getInstance().runCommand(out, entry);
    if(out.empty() && responses.empty())
        Screen::dismiss(this);
    else
    {
        is_response=true;
    }
}
void viewscreen_commandpromptst::feed(std::set<df::interface_key> *events)
{

    bool leave_all = events->count(interface_key::LEAVESCREEN_ALL);
    if (leave_all || events->count(interface_key::LEAVESCREEN))
    {
        events->clear();
        Screen::dismiss(this);
        if (leave_all)
        {
            events->insert(interface_key::LEAVESCREEN);
            parent->feed(events);
            events->clear();
        }
        return;
    }
    if(events->count(interface_key::SELECT))
    {
        submit();
        return;
    }
    if(is_response)
        return;
    for (auto it = events->begin(); it != events->end(); ++it)
    {
        auto key = *it;
        if (key==interface_key::STRING_A000) //delete?
        {
            if(entry.size() && cursor_pos > 0)
            {
                entry.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
            if(cursor_pos > entry.size())
                cursor_pos = entry.size();
            continue;
        }
        if (key >= interface_key::STRING_A000 &&
            key <= interface_key::STRING_A255)
        {
            entry.insert(cursor_pos, 1, char(key - interface_key::STRING_A000));
            cursor_pos++;
        }
        // Prevent number keys from moving cursor
        else if(events->count(interface_key::CURSOR_RIGHT))
        {
            cursor_pos++;
            if (cursor_pos > entry.size()) cursor_pos = entry.size();
            break;
        }
        else if(events->count(interface_key::CURSOR_LEFT))
        {
            cursor_pos--;
            if (cursor_pos < 0) cursor_pos = 0;
            break;
        }
        else if(events->count(interface_key::CUSTOM_CTRL_A))
        {
            cursor_pos = 0;
        }
        else if(events->count(interface_key::CUSTOM_CTRL_E))
        {
            cursor_pos = entry.size();
        }
    }
}
DFHACK_PLUGIN("command-prompt");
command_result show_prompt(color_ostream &out, std::vector <std::string> & parameters)
{
    if (Gui::getCurFocus() == "dfhack/commandprompt")
    {
        Screen::dismiss(Gui::getCurViewscreen(true));
    }
    std::string params;
    for(size_t i=0;i<parameters.size();i++)
        params+=parameters[i]+" ";
    Screen::show(new viewscreen_commandpromptst(params));
    return CR_OK;
}
bool hotkey_allow_all(df::viewscreen *top)
{
    return true;
}
DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "command-prompt","Shows a command prompt on window.",show_prompt,hotkey_allow_all,
        "command-prompt [entry] - shows a cmd prompt in df window. Entry is used for default prefix (e.g. ':lua')"
        ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}