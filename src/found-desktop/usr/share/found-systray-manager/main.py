#! /usr/bin/python3
import sys
import dbus
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gio, Gdk
import gettext
import os
import os.path
import json

gettext.bindtextdomain('found-desktop', '/usr/share/locale')
gettext.textdomain('found-desktop')
_ = gettext.gettext

SCHEMA = "org.cdos"

home = os.path.expanduser("~")

class Client():
    def __init__(self):
        bus=dbus.SystemBus()
        service = bus.get_object('org.freedesktop.UPower', "/org/freedesktop/UPower/devices/DisplayDevice");
        iface = dbus.Interface (service, dbus_interface='org.freedesktop.DBus.Properties')
        self.get_device = iface.get_dbus_method ("Get",dbus_interface=None)

    def run(self):
        try:
            devices = self.get_device ("org.freedesktop.UPower.Device","PowerSupply")
            return bool(devices)
        except:
            return False

class GroupedManager:
    def __init__(self):
        self.settings = Gio.Settings.new(SCHEMA)
        self.grouped_list = self.settings.get_strv('grouped-applets')
        self.settings.connect("changed",self._reload)

        window = Gtk.Window()
        window.set_size_request(480, 480)

        window.set_position(Gtk.WindowPosition.CENTER)
        window.set_title(_("Systray Settings"))
        window.set_default_icon_name("preferences-system")
        window.connect("destroy", lambda q: Gtk.main_quit())

        self.set_theme('/usr/share/found-systray-manager/style.css')
        self.body=Gtk.ScrolledWindow()

        self.body.get_style_context().add_class('traygroup-box')
        self.body.set_policy(Gtk.PolicyType.NEVER,Gtk.PolicyType.AUTOMATIC)

        self.listBox=Gtk.ListBox()
        self.listBox.get_style_context().add_class("traygroup-listbox")
        self.listBox.set_property("selection-mode",False)

        self.body.add(self.listBox)
        window.add(self.body)

        self._load()
        window.show_all()
        Gtk.main()

    def _load(self):

        exclude = ['show-desktop@cdos.org', 'systray@cdos.org','menu@cdos.org','scale@cdos.org','window-list@cdos.org','calendar@cdos.org','grouped-applets@cdos.org', 'removable-drives@cdos.org', 'feedback@cdos.org']
        enabled_applets = self.settings.get_strv('enabled-applets')

        list_len = len(self.grouped_list)
        for element in enabled_applets:
            uuid = element.split(':')[2]
            box = element.split(':')[0]
            if uuid in exclude:
                continue
            if uuid == 'power-menu@cdos.org' :
                continue
            metadata = self.get_metadata(uuid)
            if metadata == {}:
                continue
            it = Gtk.ListBoxRow()
            item = Gtk.Box()
            it.add(item)
            it.get_style_context().add_class("traygroup-listbox-item")
            it.set_size_request(480,50)

            selectText = Gtk.ComboBoxText()
            selectText.get_style_context().add_class("traygroup-listbox-item-select")
            selectText.set_name(uuid)

            name = metadata['name'] if metadata['name'] else uuid

            itemName = Gtk.Label(_(name))
            itemName.get_style_context().add_class("traygroup-listbox-item-label")
            item.pack_start(itemName,False,False,5)
            item.pack_end(selectText,False,False,0)
            selectText.set_size_request(140,30)

            if uuid in self.grouped_list:
                selectText.append("group",_("In Group"))
                selectText.append("panel",_("On Panel"))
            elif box == "right" and uuid not in exclude:
                selectText.append("panel",_("On Panel"))
                selectText.append("group",_("In Group"))
            selectText.set_active(0)
            devices = Client().run()
            if uuid == "power@cdos.org" and devices == False:
                selectText.set_sensitive (0)

            selectText.set_margin_right(10)

            self.listBox.insert(it,-1)
            selectText.connect("changed", self.combo_changed)

    def _reload(self,key,data):
        if key == "enabled-applets":
            self._load()

    def combo_changed(self,data):
        selectedId = data.get_active_id()
        uuid=data.get_name()
        if selectedId == "group":
            self.grouped_list.append(uuid)
        elif selectedId == "panel":
            self.grouped_list.remove(uuid)
        self.settings.set_strv('grouped-applets', list(set(self.grouped_list)))

    def set_theme(self,file_theme):
        screen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        provider.load_from_path(file_theme)
        context = Gtk.StyleContext()
        context.add_provider_for_screen(screen, provider, Gtk.STYLE_PROVIDER_PRIORITY_USER)
        self.context = context

    def get_metadata(self,uuid):
        collection_type = "applet"
        metadata = {}
        metadata = self.load_metadata_in(('%s/.local/share/found/%ss/%s') % (home, collection_type,uuid))
        if metadata == {}:
            metadata = self.load_metadata_in(('/usr/share/found-desktop/%ss/%s') % (collection_type,uuid))
        return metadata

    def load_metadata_in(self, directory):
        metadata = {}
        if os.path.exists(directory) and os.path.isdir(directory):
            if os.path.exists("%s/metadata.json" % (directory)):
                json_data = open("%s/metadata.json" % (directory)).read()
                metadata = json.loads(json_data)
        return metadata

if __name__ == "__main__":
	app = GroupedManager()
