#!/usr/bin/python3

import gettext
import os
import dbus
import fcntl
import sys
import xml.dom.minidom
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk
from xml.dom.minidom import parse

import foundtools.config as ftconfig

gettext.bindtextdomain(ftconfig.GETTEXT_PACKAGE, ftconfig.localedir)
gettext.textdomain(ftconfig.GETTEXT_PACKAGE)
_ = gettext.gettext

MIRRORED_ICON	= "/usr/share/found-monitors-switcher/icons/mirror.png"
DOCKED_ICON	= "/usr/share/found-monitors-switcher/icons/dock.png"
EXTENDED_ICON	= "/usr/share/found-monitors-switcher/icons/extension.png"
INTERNAL_ICON	= "/usr/share/found-monitors-switcher/icons/internal.png"

#CONFIG_FILE = "/usr/share/found-monitors-switcher/mode.config"
#CONFIG_FILE = os.environ['HOME']+"/.config/mode.config"
RESOLUTION_FILE = os.environ['HOME'] + "/.config/monitors.xml"
print(RESOLUTION_FILE)
#CONFIG_FILE = "/usr/share/found-monitors-switcher/mode.config"
CONFIG_FILE = os.environ['HOME'] + "/.config/mode.config"
print(CONFIG_FILE)
GRAY_MIRRORED_ICON	= "/usr/share/found-monitors-switcher/icons/gray_mirror.png"
GRAY_DOCKED_ICON	= "/usr/share/found-monitors-switcher/icons/gray_dock.png"
GRAY_EXTENDED_ICON	= "/usr/share/found-monitors-switcher/icons/gray_extension.png"
GRAY_INTERNAL_ICON	= "/usr/share/found-monitors-switcher/icons/gray_internal.png"

def read_resolution(lvds, vga):
    #print lvds, vga
    try:
        xmldoc = xml.dom.minidom.parse(RESOLUTION_FILE)
        monitors = xmldoc.documentElement
    except Exception as e:
        print(("Warning: cannot parse file: %s" %RESOLUTION_FILE))
        return ("", "")
    lvds_resolution=""
    vga_resolution=""
    outputs = monitors.getElementsByTagName("output")
    for output in outputs:
        if output.childNodes.length <= 1:
            continue
        if output.hasAttribute("name"):
            #print "output name: %s" % output.getAttribute("name")
            name = output.getAttribute("name")
            if name == lvds:
                if output.hasAttribute("width"):
                    lvds_width = output.getElementsByTagName('width')[0]
                    lvds_height = output.getElementsByTagName('height')[0]
                    lvds_resolution = str(lvds_width.childNodes[0].data) + "x" + str(lvds_height.childNodes[0].data)
                else:
                    vga_resolution = str("1024") + "x" + str("768")
            if name == vga:
                if output.hasAttribute("width"):
                    vga_width = output.getElementsByTagName('width')[0]
                    vga_height = output.getElementsByTagName('height')[0]
                    vga_resolution = str(vga_width.childNodes[0].data) + "x" + str(vga_height.childNodes[0].data)
                else:
                    vga_resolution = str("1024") + "x" + str("728")

    if(lvds_resolution and vga_resolution):
        return (lvds_resolution, vga_resolution)
    else:
        return ("", "")

def get_primary(lvds, vga):
    #print lvds, vga
    try:
        xmldoc = xml.dom.minidom.parse(RESOLUTION_FILE)
        monitors = xmldoc.documentElement
    except Exception as e:
        print(("Warning: cannot parse file: %s" %RESOLUTION_FILE))
        return ""
    outputs = monitors.getElementsByTagName("output")
    for output in outputs:
        if output.childNodes.length <= 1:
            continue
        if output.hasAttribute("name"):
            #print "output name: %s" % output.getAttribute("name")
            name = output.getAttribute("name")
            if output.hasAttribute("primary"):
                primary_out = output.getElementsByTagName("primary")[0]
                res = str(primary_out.childNodes[0].data)
            else:
                return "lvds"
            #print res
            if res == "yes":
                return name
    return ""

def update_config(str):
    # print "str:%s" %str
    fcon = open(CONFIG_FILE, "w")
    try:
        fcon.write(str)
    finally:
        fcon.close()

def read_config():
    config_curr = ""
    fcon = open(CONFIG_FILE, "a+")
    try:
        config_curr = fcon.read().strip('\n')
    finally:
        fcon.close()

    return config_curr


BLUE_MIRRORED_ICON	= "/usr/share/found-monitors-switcher/icons/blue_mirror.png"
BLUE_DOCKED_ICON	= "/usr/share/found-monitors-switcher/icons/blue_dock.png"
BLUE_EXTENDED_ICON	= "/usr/share/found-monitors-switcher/icons/blue_extension.png"
BLUE_INTERNAL_ICON	= "/usr/share/found-monitors-switcher/icons/blue_internal.png"

#MIRRORED_ICON	= "./icons/mirror.png"
#DOCKED_ICON	= "./icons/dock.png"
#EXTENDED_ICON	= "./icons/extension.png"
#INTERNAL_ICON	= "./icons/internal.png"

#GRAY_MIRRORED_ICON	= "./icons/gray_mirror.png"
#GRAY_DOCKED_ICON	= "./icons/gray_dock.png"
#GRAY_EXTENDED_ICON	= "./icons/gray_extension.png"
#GRAY_INTERNAL_ICON	= "./icons/gray_internal.png"

BLUE_MIRRORED_ICON	= "./icons/blue_mirror.png"
BLUE_DOCKED_ICON	= "./icons/blue_dock.png"
BLUE_EXTENDED_ICON	= "./icons/blue_extension.png"
BLUE_INTERNAL_ICON	= "./icons/blue_internal.png"



# Singleton
fh = 0
def run_once():
    global fh

    fh = open(os.path.realpath(__file__), 'r')

    try:
        fcntl.flock(fh, fcntl.LOCK_EX|fcntl.LOCK_NB)
    except:
        os._exit(0)
    finally:
        fh.close()
'''
def get_connectors():
    #vga_output = os.popen('xrandr | grep VGA | grep " connected " | awk \'{print $1}\'')
    #vga = vga_output.read().strip('\n')
    #crt_output = os.popen('xrandr | grep CRT | grep " connected " | awk \'{print $1}\'')
    #crt = crt_output.read().strip('\n')
    vga_output = os.popen("xrandr | grep VGA | grep ' connected ' | awk '{print $1}'")
    vga = vga_output.read().strip('\n')
    crt_output = os.popen("xrandr | grep -e CRT -e LVDS -e DP | grep ' connected ' | awk '{print $1}' | head -n1")
    crt = crt_output.read().strip('\n')

    if ((not vga.strip()) and crt):
        vga = crt

    lvds_output = os.popen("xrandr | grep CRT | grep ' connected ' | awk '{print $1}'")
    lvds = lvds_output.read().strip('\n')
    dp_output = os.popen("xrandr | grep -e DP -e HDMI | grep ' connected ' | awk '{print $1}' | head -n1")
    dp = dp_output.read().strip('\n')
    #if ((not lvds.strip()) and dp):
    if (dp):
        lvds = dp

    dp5_output = os.popen("xrandr | grep DP5 | grep ' connected ' | awk '{print $1}'")
    dp5 = dp5_output.read().strip('\n')
    dp6_output = os.popen("xrandr | grep DP6 | grep ' connected ' | awk '{print $1}'")
    dp6 = dp6_output.read().strip('\n')
    if (dp5):
        lvds_output = os.popen("xrandr | grep CRT | grep ' connected ' | awk '{print $1}'")
        lvds = lvds_output.read().strip('\n')
        if (lvds):
            vga = dp5
    if (dp5 and dp6):
        lvds = dp6
        vga = dp5
    dp1_output = os.popen("xrandr | grep DP1 | grep ' connected ' | awk '{print $1}'")
    dp1 = dp1_output.read().strip('\n')
    dp2_output = os.popen("xrandr | grep DP2 | grep ' connected ' | awk '{print $1}'")
    dp2 = dp2_output.read().strip('\n')
    if (dp1 and dp2):
        lvds = dp2
        vga = dp1

    print "test",vga,lvds
    return (vga, lvds)
'''
def get_connectors():
    vga_output = os.popen("xrandr | grep connected | grep -v disconnected | awk '{print $1}' | awk 'NR==1'")
    vga = vga_output.read().strip('\n')

    monitors_num_output = os.popen("xrandr | grep connected | grep -v disconnected | awk '{print $1}' | wc -l")
    monitors_num = monitors_num_output.read().strip('\n')

    if(monitors_num == "2"):
        lvds_output = os.popen("xrandr | grep connected | grep -v disconnected | awk '{print $1}'| awk 'NR==2'")
        lvds = lvds_output.read().strip('\n')
    else:
        lvds = vga

    print((monitors_num,vga,lvds))
    return vga,lvds

def get_best_resolution():
    res = os.popen('/bin/bash /usr/share/found-monitors-switcher/adjust.sh')
    r=res.read().strip('\n').split('|')
    return r

def get_single_best_resolution(single):
    res = os.popen('/bin/bash /usr/share/found-monitors-switcher/adjust.sh ' + single)
    r=res.read().strip('\n')
    return r


def get_ext_best_resolution(lvds, vga):
    res = os.popen('/bin/bash /usr/share/found-monitors-switcher/adjust.sh ' + lvds + ' ' + vga)
    r=res.read().strip('\n').split('|')
    return r


class CMSWindow(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self, title=_("Cinnamon Multi-Monitor Switcher"))
        # Main window conf
        self.set_border_width(10)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_default_size(350, 270)
        self.set_resizable(False)
        self.set_decorated(False)
        #		self.set_opacity(0.8)
        self.set_icon_name("backgrounds")

        # dbus init
        #		bus = dbus.SessionBus()
        #		bus_obj = bus.get_object('org.gnome.SettingsDaemon', '/org/cdos/SettingsDaemon/XRANDR')
        #		self.iface = dbus.Interface(bus_obj, 'org.cdos.SettingsDaemon.XRANDR_2')

        # drawing GUI stuff
        hbox = Gtk.Box(spacing=6)
        self.add(hbox)

        # Internal mode button
        self.i_button = Gtk.Button()
        self.i_button.connect("clicked", self.on_internal_button_clicked)
        i_image = Gtk.Image()
        i_image.set_from_file(INTERNAL_ICON)
        self.i_button.add(i_image)
        hbox.pack_start(self.i_button, True, True, 0)

        # Mirrored mode button
        self.m_button = Gtk.Button()
        self.m_button.connect("clicked", self.on_mirrored_button_clicked)
        m_image = Gtk.Image()
        m_image.set_from_file(MIRRORED_ICON)
        self.m_button.add(m_image)

        hbox.pack_start(self.m_button, True, True, 0)

        # Docked monitor mode button
        self.d_button = Gtk.Button()
        self.d_button.connect("clicked", self.on_docked_button_clicked)
        d_image = Gtk.Image()
        d_image.set_from_file(DOCKED_ICON)
        self.d_button.add(d_image)

        hbox.pack_start(self.d_button, True, True, 0)

        # extended mode button
        self.e_button = Gtk.Button()
        self.e_button.connect("clicked", self.on_extended_button_clicked)
        e_image = Gtk.Image()
        e_image.set_from_file(EXTENDED_ICON)
        self.e_button.add(e_image)

        hbox.pack_start(self.e_button, True, True, 0)

        self.connect("key_press_event", self.key_press_event)

        mode = read_config()
        if mode == "":
            print("mode is null")
            mode = "4"
        # else:
        #     print mode
        if (mode == "1"):
            # self.set_focus(self.i_button)
            self.i_button.grab_focus()
            childimage = self.i_button.get_child()
            childimage.set_from_file(GRAY_INTERNAL_ICON)
        elif (mode == "2"):
            # self.set_focus(self.m_button)
            self.m_button.grab_focus()
            childimage = self.m_button.get_child()
            childimage.set_from_file(GRAY_MIRRORED_ICON)
        elif (mode == "3"):
            # self.set_focus(self.d_button)
            self.d_button.grab_focus()
            childimage = self.d_button.get_child()
            childimage.set_from_file(GRAY_DOCKED_ICON)
        elif (mode == "4"):
            # self.set_focus(self.e_button)
            self.e_button.grab_focus()
            childimage = self.e_button.get_child()
            childimage.set_from_file(GRAY_EXTENDED_ICON)

    def do_switch_internal(self):
        vga, lvds = get_connectors()
        print(("internal",vga,lvds))
        #print("lvfei switch internal: test if file exist if exist,read it,if not use auto  ")
        resolution_lvds, resolution_vga = read_resolution(lvds, vga)
        #print resolution_lvds
        #print resolution_vga
        if (lvds and vga and lvds != vga):
            #make all the audio to the internal analog audio sink
            os.system("/bin/bash /usr/share/found-monitors-switcher/audio2analog.sh")
            #resolution_lvds, resolution_vga = read_resolution(lvds, vga)
            #os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
            is_exist = os.path.exists(RESOLUTION_FILE)
            #Lawson if(is_exist and resolution_lvds):
            if(is_exist and resolution_vga):
                #Lawson os.system("xrandr --output %s --off --output %s --mode %s" % (vga,lvds,resolution_lvds))
                os.system("xrandr --output %s --off --output %s --mode %s" % (lvds,vga,resolution_vga))
                #print("internal mode")
                update_config("1")
            else:
                #os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
                os.system("xrandr --output %s --off --output %s --auto" % (lvds, vga))
                update_config("1")
        else:
            print("nothing... internal mode button pressed..")
            #os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
            res1 = get_single_best_resolution(vga)
            print(("switch internal sinle",res1))
            if (res1):
                print((vga,lvds))
                print((lvds,vga,res1))
                os.system("xrandr --output %s --off --output %s --mode %s" % (lvds,vga,res1))
            #else:
            #    os.system("xrandr --output %s --off --output %s --auto" % (lvds, vga))
            update_config("1")

    def do_switch_mirror(self):
        vga, lvds = get_connectors()
        if (lvds and vga and lvds != vga):
            res1, res2 = get_best_resolution()
            #print res1,res2
            #os.system("xrandr --output %s --auto --output %s --off" % (vga, lvds))
            os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
            os.system("xrandr --output %s --mode %s --primary --output %s --mode %s --same-as %s" % (lvds, res1, vga, res2, lvds))
            #print ("mirrored mode")
            update_config("2")
        else:
            print("nothing... mirrored mode button pressed..")

    def do_switch_docked(self):
        vga, lvds = get_connectors()
        resolution_lvds, resolution_vga = read_resolution(lvds, vga)
        #print ("in dock test the file is exist,if exist,read it ,if not use best ")
        #print resolution_lvds
        if (lvds and vga and lvds != vga):
            is_exist = os.path.exists(RESOLUTION_FILE)
            #if(is_exist and resolution_vga):
            if(is_exist and resolution_lvds):
                print("external mode1")
                #Lawson os.system("xrandr --output %s --off --output %s --mode %s" % (lvds,vga,resolution_vga))
                os.system("xrandr --output %s --off --output %s --mode %s" % (vga,lvds,resolution_lvds))
                #os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
                #os.system("xrandr --output %s --auto --output %s --off" % (vga, lvds))
                update_config("3")
            else:
                # Lawson ZXE Tiny probability logout when switching display mode
                # os.system("xrandr --output %s --auto --output %s --off" % (vga, lvds))
                os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
                #os.system("xrandr --output %s --auto --output %s --off" % (vga, lvds))
                #print ("update the  dock status")
                update_config("3")
            print("external mode")
            #update_config("1")
        else:
            print("nothing... docked mode button pressed..")
            res1 = get_single_best_resolution(lvds)
            if (res1):
                os.system("xrandr --output %s --off --output %s --mode %s" % (vga,lvds,res1))
            #else:
            #    os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
 
            #os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
            #os.system("xrandr --output %s --auto --output %s --off" % (vga, lvds))
            update_config("3")

    def do_switch_extended(self):
        vga, lvds = get_connectors()
        primary_out = get_primary(lvds, vga)
        #print ("lvfei is vga:%s,lvds:%s") %(vga,lvds)
        #print "primary output is %s" %primary_out
        if (lvds and vga and lvds != vga):
            #Lawson resolution_lvds, resolution_vga = read_resolution(lvds, vga)
            resolution_lvds, resolution_vga = get_ext_best_resolution(lvds, vga)
            os.system("xrandr --output %s --output %s" % (vga, lvds))
            if(resolution_lvds and resolution_vga ):
                if lvds == primary_out:
                    print((lvds, resolution_lvds, vga, resolution_vga, lvds))
                    os.system("xrandr --output %s --mode %s --primary --output %s --mode %s --right-of %s" % (lvds, resolution_lvds, vga, resolution_vga, lvds))
                else:
                    os.system("xrandr --output %s --mode %s --primary --output %s --mode %s --right-of %s" % (vga, resolution_vga, lvds, resolution_lvds, vga))

            else:
                os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
                if(lvds == "CRT1"):
                    os.system("xrandr --output %s --auto --primary --output %s --auto --right-of %s" % (vga, lvds, vga))
                else:
                    os.system("xrandr --output %s --auto --primary --output %s --auto --right-of %s" % (lvds, vga, lvds))
            #print("extended mode.")
            print((lvds, resolution_lvds, vga, resolution_vga, lvds))
            update_config("4")
        else:
            print("nothing... extended mode button pressed..")
            res1 = get_single_best_resolution(lvds)
            if (res1):
                os.system("xrandr --output %s --off --output %s --mode %s" % (vga,lvds,res1))
            #else:
            #    os.system("xrandr --output %s --off --output %s --auto" % (vga, lvds))
 
    def on_internal_button_clicked(self, widget):
        self.do_switch_internal()
        Gtk.main_quit()

    def on_mirrored_button_clicked(self, widget):
        self.do_switch_mirror()
        Gtk.main_quit()

    def on_docked_button_clicked(self, widget):
        self.do_switch_docked()
        Gtk.main_quit()

    def on_extended_button_clicked(self, widget):
        self.do_switch_extended()
        Gtk.main_quit()

    def key_press_event(self, widget, event):
        global timer_interval
        timer_interval=0
        keyname = Gdk.keyval_name(event.keyval)
        #print "Key %s (%d) was pressed" % (keyname, event.keyval)
        if(keyname == "Escape"):
            Gtk.main_quit()
        if((keyname =="p") or (keyname =="F10")):
            if(self.i_button==self.get_focus()):
                # print "i_button get_focus"
                # self.set_focus(self.m_button)
                self.m_button.grab_focus()
                childimage = self.m_button.get_child()
                childimage.set_from_file(GRAY_MIRRORED_ICON)
                childimage = self.i_button.get_child()
                childimage.set_from_file(INTERNAL_ICON)
            elif(self.m_button==self.get_focus()):
                # print "m_button get_focus"
                # self.set_focus(self.d_button)
                self.d_button.grab_focus()
                childimage = self.d_button.get_child()
                childimage.set_from_file(GRAY_DOCKED_ICON)
                childimage = self.m_button.get_child()
                childimage.set_from_file(MIRRORED_ICON)
            elif(self.d_button==self.get_focus()):
                # print "d_button get_focus"
                # self.set_focus(self.e_button)
                self.e_button.grab_focus()
                childimage = self.e_button.get_child()
                childimage.set_from_file(GRAY_EXTENDED_ICON)
                childimage = self.d_button.get_child()
                childimage.set_from_file(DOCKED_ICON)
            elif(self.e_button==self.get_focus()):
                # print "e_button get_focus"
                # self.set_focus(self.i_button)
                self.i_button.grab_focus()
                childimage = self.i_button.get_child()
                childimage.set_from_file(GRAY_INTERNAL_ICON)
                childimage = self.e_button.get_child()
                childimage.set_from_file(EXTENDED_ICON)
        elif(keyname=="Right"):
            if(self.i_button==self.get_focus()):
                # self.set_focus(self.m_button)
                childimage = self.m_button.get_child()
                childimage.set_from_file(GRAY_MIRRORED_ICON)
                childimage = self.i_button.get_child()
                childimage.set_from_file(INTERNAL_ICON)
            elif(self.m_button==self.get_focus()):
                # self.set_focus(self.d_button)
                childimage = self.d_button.get_child()
                childimage.set_from_file(GRAY_DOCKED_ICON)
                childimage = self.m_button.get_child()
                childimage.set_from_file(MIRRORED_ICON)
            elif(self.d_button==self.get_focus()):
                # self.set_focus(self.e_button)
                childimage = self.e_button.get_child()
                childimage.set_from_file(GRAY_EXTENDED_ICON)
                childimage = self.d_button.get_child()
                childimage.set_from_file(DOCKED_ICON)
            elif(self.e_button==self.get_focus()):
                # self.set_focus(self.i_button)
                childimage = self.e_button.get_child()
                childimage.set_from_file(GRAY_EXTENDED_ICON)
                # childimage = self.i_button.get_child()
                # childimage.set_from_file(GRAY_INTERNAL_ICON)
        elif(keyname=="Left"):
            if(self.i_button==self.get_focus()):
                # self.set_focus(self.m_button)
                # childimage = self.m_button.get_child()
                # childimage.set_from_file(INTERNAL_ICON)
                childimage = self.i_button.get_child()
                childimage.set_from_file(GRAY_INTERNAL_ICON)
            elif(self.m_button==self.get_focus()):
                # self.set_focus(self.d_button)
                childimage = self.i_button.get_child()
                childimage.set_from_file(GRAY_INTERNAL_ICON)
                childimage = self.m_button.get_child()
                childimage.set_from_file(MIRRORED_ICON)
            elif(self.d_button==self.get_focus()):
                # self.set_focus(self.e_button)
                childimage = self.m_button.get_child()
                childimage.set_from_file(GRAY_MIRRORED_ICON)
                childimage = self.d_button.get_child()
                childimage.set_from_file(DOCKED_ICON)
            elif(self.e_button==self.get_focus()):
                # self.set_focus(self.i_button)
                childimage = self.d_button.get_child()
                childimage.set_from_file(GRAY_DOCKED_ICON)
                childimage = self.e_button.get_child()
                childimage.set_from_file(EXTENDED_ICON)

run_once()

win = CMSWindow()
win.connect("delete-event", Gtk.main_quit)
win.connect("focus-out-event", Gtk.main_quit)

if( len(sys.argv) > 1):
    mode=sys.argv[1]
    print(mode)
    if (mode == "1"):
        win.do_switch_internal()
    elif (mode == "2"):
        win.do_switch_mirror()
    elif (mode == "3"):
        win.do_switch_docked()
    elif (mode == "4"):
        win.do_switch_extended()
else:
    print("ok")
    win.show_all()
    Gtk.main()

