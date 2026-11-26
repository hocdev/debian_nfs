#!/usr/bin/env python3
#coding: utf-8

try:
    import os
    import os.path
    import subprocess
    import sys
    import string
    import gettext
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gio, Gtk, GObject, Gdk, GLib, Pango
    from gi.repository import GdkPixbuf 
    import dbus
    import time
    from datetime import datetime
    import urllib.request, urllib.parse, urllib.error
    import lxml.etree
    import locale    
    import imtools
    from PIL import Image
    import math
    import re

except Exception as detail:
    print(detail)
    sys.exit(1)

settings_objects = {}

class BaseChooserButton(Gtk.Button):
    def __init__ (self, has_button_label=False):
        super(BaseChooserButton, self).__init__()
        self.menu = Gtk.Menu()
        self.button_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        self.button_image = Gtk.Image()
        self.button_box.add(self.button_image)
        if has_button_label:
            self.button_label = Gtk.Label()
            self.button_box.add(self.button_label)
        self.add(self.button_box)
        self.connect("button-release-event", self._on_button_clicked)

    def popup_menu_below_button (self, *args):
        # the introspection for GtkMenuPositionFunc seems to change with each Gtk version,
        # this is a workaround to make sure we get the menu and the widget
        menu = args[0]
        widget = args[-1]
        window = widget.get_window()
        screen = window.get_screen()
        monitor = screen.get_monitor_at_window(window)

        warea = screen.get_monitor_workarea(monitor)
        wrect = widget.get_allocation()
        mrect = menu.get_allocation()

        unused_var, window_x, window_y = window.get_origin()

        # Position left edge of the menu with the right edge of the button
        x = window_x + wrect.x + wrect.width
        # Center the menu vertically with respect to the monitor
        y = warea.y + (warea.height / 2) - (mrect.height / 2)

        # Now, check if we're still touching the button - we want the right edge
        # of the button always 100% touching the menu

        if y > (window_y + wrect.y):
            y = y - (y - (window_y + wrect.y))
        elif (y + mrect.height) < (window_y + wrect.y + wrect.height):
            y = y + ((window_y + wrect.y + wrect.height) - (y + mrect.height))

        push_in = True # push_in is True so all menu is always inside screen
        return (x, y, push_in)

    def _on_button_clicked(self, widget, event):
        if event.button == 1:
            self.menu.show_all()
            self.menu.popup(None, None, self.popup_menu_below_button, self, event.button, event.time)

class PictureChooserButton(BaseChooserButton):
    def __init__ (self, num_cols=4, button_picture_size=None, menu_pictures_size=None, has_button_label=False):
        super(PictureChooserButton, self).__init__(has_button_label)
        self.num_cols = num_cols
        self.button_picture_size = button_picture_size
        self.menu_pictures_size = menu_pictures_size
        self.row = 0
        self.col = 0
        self.progress = 0.0

        context = self.get_style_context()
        context.add_class("gtkstyle-fallback")

        self.connect_after("draw", self.on_draw)

    def on_draw(self, widget, cr, data=None):
        if self.progress == 0:
            return False
        box = widget.get_allocation()

        context = widget.get_style_context()
        c = context.get_background_color(Gtk.StateFlags.SELECTED)

        max_length = box.width * .6
        start = (box.width - max_length) / 2
        y = box.height - 5

        cr.save()

        cr.set_source_rgba(c.red, c.green, c.blue, c.alpha)
        cr.set_line_width(3)
        cr.set_line_cap(1)
        cr.move_to(start, y)
        cr.line_to(start + (self.progress * max_length), y)
        cr.stroke()

        cr.restore()
        return False

    def increment_loading_progress(self, inc):
        progress = self.progress + inc
        self.progress = min(1.0, progress)
        self.queue_draw()

    def reset_loading_progress(self):
        self.progress = 0.0
        self.queue_draw()

    def set_picture_from_file (self, path):
        pixbuf = None
        if os.path.exists(path):
            if self.button_picture_size is None:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file(path)
            else:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(path, -1, self.button_picture_size, True)
        else:
            icon_theme = Gtk.IconTheme()
            if self.button_picture_size is None:
                pixbuf = icon_theme.load_icon("image-missing", 48, Gtk.IconLookupFlags.FORCE_SVG)
            else:
                pixbuf = icon_theme.load_icon("image-missing", self.button_picture_size, Gtk.IconLookupFlags.FORCE_SVG)
        self.button_image.set_from_pixbuf(pixbuf)

    def set_button_label(self, label):
        self.button_label.set_markup(label)

    def _on_picture_selected(self, menuitem, path, callback, id=None):
        if id is not None:
            result = callback(path, id)
        else:
            result = callback(path)

        if result:
            self.set_picture_from_file(path)

    def clear_menu(self):
        menu = self.menu
        self.menu = Gtk.Menu()
        self.row = 0
        self.col = 0
        menu.destroy()

    def add_picture(self, path, callback, title=None, id=None):
        image = None
        if os.path.exists(path):
            if self.menu_pictures_size is None:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file(path)
            else:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(path, -1, self.menu_pictures_size, True)
            image = Gtk.Image.new_from_pixbuf (pixbuf)
        else:
            icon_theme = Gtk.IconTheme()
            pixbuf = icon_theme.load_icon("image-missing", 48, Gtk.IconLookupFlags.FORCE_SVG)
            image = Gtk.Image.new_from_pixbuf (pixbuf)

        menuitem = Gtk.MenuItem()
        if title is not None:
            vbox = Gtk.VBox()
            vbox.pack_start(image, False, False, 2)
            label = Gtk.Label()
            label.set_text(title)
            vbox.pack_start(label, False, False, 2)
            menuitem.add(vbox)
        else:
            menuitem.add(image)
        if id is not None:
            menuitem.connect('activate', self._on_picture_selected, path, callback, id)
        else:
            menuitem.connect('activate', self._on_picture_selected, path, callback)
        self.menu.attach(menuitem, self.col, self.col+1, self.row, self.row+1)
        self.col = (self.col+1) % self.num_cols
        if (self.col == 0):
            self.row = self.row + 1

    def add_separator(self):
        self.row = self.row + 1
        self.menu.attach(Gtk.SeparatorMenuItem(), 0, self.num_cols, self.row, self.row+1)

    def add_menuitem(self, menuitem):
        self.row = self.row + 1
        self.menu.attach(menuitem, 0, self.num_cols, self.row, self.row+1)

class SidePage:
    def __init__(self, name, icon, keywords, content_box = None, size = None, is_c_mod = False, is_standalone = False, exec_name = None, module=None):
        self.name = name
        self.icon = icon
        self.content_box = content_box
        self.widgets = []
        self.is_c_mod = is_c_mod
        self.is_standalone = is_standalone
        self.exec_name = exec_name
        self.module = module # Optionally set by the module so we can call on_module_selected() on it when we show it.
        self.keywords = keywords
        self.size = size
        self.topWindow = None
        self.builder = None
        if self.module != None:
            self.module.loaded = False

    def add_widget(self, widget):
        self.widgets.append(widget)        

    def build(self):        
        # Clear all the widgets from the content box
        widgets = self.content_box.get_children()
        for widget in widgets:
            self.content_box.remove(widget)

        if (self.module is not None):
            self.module.on_module_selected()
            self.module.loaded = True

        # Add our own widgets
        # C modules are sort of messy - they check the desktop type
        # (for Unity or GNOME) and show/hide UI items depending on
        # the result - so we cannot just show_all on the widget, it will
        # mess up these modifications - so for these, we just show the
        # top-level widget
        if not self.is_standalone:
            for widget in self.widgets:
                if hasattr(widget, 'expand'):
                    self.content_box.pack_start(widget, True, True, 2)
                else:
                    self.content_box.pack_start(widget, False, False, 2)
            if self.is_c_mod:
                self.content_box.show()
                children = self.content_box.get_children()
                for child in children:
                    child.show()
                    if child.get_name() == "c_box":
                        c_widgets = child.get_children()
                        if not c_widgets:
                            c_widget = self.content_box.c_manager.get_c_widget(self.exec_name)
                            if c_widget is not None:
                                child.pack_start(c_widget, False, False, 2)
                                c_widget.show()
                        else:
                            for c_widget in c_widgets:
                                c_widget.show()
            else:
                self.content_box.show_all()
                try:
                    self.check_third_arg()
                except:
                    pass
        else:
            subprocess.Popen(self.exec_name.split())

class Section(Gtk.Box):
    def __init__(self, name):
        self.name = name
        super(Section, self).__init__()
        self.set_orientation(Gtk.Orientation.VERTICAL)
        self.set_border_width(6)
        self.set_spacing(6)
        self.label = Gtk.Label()
        self.label.set_markup("<b>%s</b>" % self.name)
        hbox = Gtk.Box()
        hbox.set_orientation(Gtk.Orientation.HORIZONTAL)
        hbox.pack_start(self.label, False, False, 0)
        self.pack_start(hbox, False, True, 0)

    def add(self, widget):
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.set_margin_left(40)
        box.set_margin_right(40)
        box.pack_start(widget, False, True, 0)
        self.pack_start(box, False, False, 0)

    def add_expand(self, widget):
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.set_margin_left(40)
        box.set_margin_right(40)
        box.pack_start(widget, True, True, 0)
        self.pack_start(box, False, False, 0)

    def add_indented(self, widget):
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.set_margin_left(80)
        box.set_margin_right(10)
        box.pack_start(widget, False, True, 0)
        self.pack_start(box, False, False, 0)

    def add_indented_expand(self, widget):
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.set_margin_left(80)
        box.set_margin_right(10)
        box.pack_start(widget, True, True, 0)
        self.pack_start(box, False, False, 0)

class SectionBg(Gtk.Viewport):
    def __init__(self):
        Gtk.Viewport.__init__(self)
        self.set_shadow_type(Gtk.ShadowType.ETCHED_IN)
        style = self.get_style_context()
        style.add_class("section-bg")
        self.expand = True # Tells CS to give expand us to the whole window


class SettingsPage(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self)
        self.set_orientation(Gtk.Orientation.VERTICAL)
        self.set_spacing(15)
        self.set_margin_left(80)
        self.set_margin_right(80)
        self.set_margin_top(15)
        self.set_margin_bottom(15)

    def add_section(self, title):
        section = SettingsBox(title)
        self.pack_start(section, False, False, 0)

        return section

    def add_reveal_section(self, title, schema=None, key=None):
        section = SettingsBox(title)
        revealer = SettingsRevealer(schema, key)
        revealer.add(section)
        self.pack_start(revealer, False, False, 0)

        return section

class SettingsBox(Gtk.Frame):
    def __init__(self, title):
        Gtk.Frame.__init__(self)
        self.set_shadow_type(Gtk.ShadowType.IN)
        frame_style = self.get_style_context()
        frame_style.add_class("view")
        self.size_group = Gtk.SizeGroup()
        self.size_group.set_mode(Gtk.SizeGroupMode.VERTICAL)

        self.box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.add(self.box)

        toolbar = Gtk.Toolbar.new()
        toolbar_context = toolbar.get_style_context()
        Gtk.StyleContext.add_class(Gtk.Widget.get_style_context(toolbar), "cs-header")

        label = Gtk.Label()
        label.set_markup("<b>%s</b>" % title)
        title_holder = Gtk.ToolItem()
        title_holder.add(label)
        toolbar.add(title_holder)
        self.box.add(toolbar)

        toolbar_separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        self.box.add(toolbar_separator)
        separator_context = toolbar_separator.get_style_context()
        frame_color = frame_style.get_border_color(Gtk.StateFlags.NORMAL).to_string()
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(".separator { -GtkWidget-wide-separators: 0; \
                                                   color: %s;                    \
                                                }" % frame_color)
        separator_context.add_provider(css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        self.need_separator = False

    def add_row(self, widget):
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        if self.need_separator:
            vbox.add(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))
        list_box = Gtk.ListBox()
        list_box.set_selection_mode(Gtk.SelectionMode.NONE)
        row = Gtk.ListBoxRow()
        row.add(widget)
        if isinstance(widget, GSettingsSwitch):
            list_box.connect("row-activated", widget.clicked)
        list_box.add(row)
        vbox.add(list_box)
        self.box.add(vbox)

        self.need_separator = True

    def add_reveal_row(self, widget, schema=None, key=None, values=None):
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        if self.need_separator:
            vbox.add(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))
        list_box = Gtk.ListBox()
        list_box.set_selection_mode(Gtk.SelectionMode.NONE)
        row = Gtk.ListBoxRow()
        row.add(widget)
        if isinstance(widget, GSettingsSwitch):
            list_box.connect("row-activated", widget.clicked)
        list_box.add(row)
        vbox.add(list_box)
        revealer = SettingsRevealer(schema, key, values)
        widget.revealer = revealer
        revealer.add(vbox)
        self.box.add(revealer)

        self.need_separator = True

        return revealer

class SettingsWidget(Gtk.Box):
    def __init__(self, dep_key=None):
        Gtk.Box.__init__(self)
        self.set_orientation(Gtk.Orientation.HORIZONTAL)
        self.set_spacing(20)
        self.set_border_width(5)
        self.set_margin_left(20)
        self.set_margin_right(20)

        if dep_key:
            flag = Gio.SettingsBindFlags.GET
            if dep_key[0] == "!":
                dep_key = dep_key[1:]
                flag |= Gio.Settings.BindFlags.INVERT_BOOLEAN

            split = dep_key.split("/")
            dep_settings = Gio.Settings.new(split[0])
            dep_settings.bind(split[1], self, "sensitive", flag)

    def add_to_size_group(self, group):
        group.add_widget(self.content_widget)

    def fill_row(self):
        self.set_border_width(0)
        self.set_margin_left(0)
        self.set_margin_right(0)

    def get_settings(self, schema):
        global settings_objects
        try:
            return settings_objects[schema]
        except:
            settings_objects[schema] = Gio.Settings.new(schema)
            return settings_objects[schema]

class IndentedHBox(Gtk.HBox):
    def __init__(self):
        super(IndentedHBox, self).__init__()
        indent = Gtk.Label.new('\t')
        self.pack_start(indent, False, False, 0)

    def add(self, item):
        self.pack_start(item, False, True, 0)

    def add_expand(self, item):
        self.pack_start(item, True, True, 0)

class GBindCheckButton(Gtk.CheckButton):
    def __init__(self, label, settings, key, dep_key):
        self.settings = settings
        self.dep_key = dep_key
        super(GBindCheckButton, self).__init__(label = label)
        self.set_active(settings.get_boolean(key))
        settings.bind(key, self, "active", Gio.SettingsBindFlags.DEFAULT)

class GSettingsCheckButton(Gtk.CheckButton):    
    def __init__(self, label, schema, key, dep_key):
        self.key = key
        self.dep_key = dep_key
        super(GSettingsCheckButton, self).__init__(label = label)
        self.settings = Gio.Settings.new(schema)        
        self.set_active(self.settings.get_boolean(self.key))
        self.settings.connect("changed::"+self.key, self.on_my_setting_changed)
        self.connectorId = self.connect('toggled', self.on_my_value_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_setting_changed(self, settings, key):
        self.disconnect(self.connectorId)                     #  panel-edit-mode can trigger changed:: twice in certain instances,
        self.set_active(self.settings.get_boolean(self.key))  #  so disconnect temporarily when we are simply updating the widget state
        self.connectorId = self.connect('toggled', self.on_my_value_changed)

    def on_my_value_changed(self, widget):
        tepkey = "workspaces-only-on-primary"
        if self.key == tepkey:
            d = Gtk.MessageDialog(None, Gtk.DialogFlags.DESTROY_WITH_PARENT, Gtk.MessageType.QUESTION, Gtk.ButtonsType.YES_NO,None)
#            d.set_keep_above(True)
            d.set_markup(_("Change after setting need to restart the desktop to take effect , restart now?"))
#            d.show_all()
            resp = d.run()
            d.destroy()
#            if resp == Gtk.ResponseType.NO:
#                return
            self.settings.set_boolean(self.key, self.get_active())
            if resp == Gtk.ResponseType.YES:
                subprocess.Popen('cdos --replace', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            return
        self.settings.set_boolean(self.key, self.get_active())

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsSwitch(SettingsWidget):
    def __init__(self, label, schema=None, key=None, dep_key=None):
        self.key = key
        super(GSettingsSwitch, self).__init__(dep_key=dep_key)

        self.content_widget = Gtk.Switch()
        self.label = Gtk.Label(label)
        self.pack_start(self.label, False, False, 0)
        self.pack_end(self.content_widget, False, False, 0)

        if schema:
            self.settings = self.get_settings(schema)
            self.settings.bind(key, self.content_widget, "active", Gio.SettingsBindFlags.DEFAULT)

    def clicked(self, listbox, row, data=None):
        self.content_widget.set_active(not self.content_widget.get_active())

class GScaleSpinButton(Gtk.HBox):    
    def __init__(self, label, schema, key, min, max, step, page, units):
        self.key = key
        self.min = min
        self.max = max
        super(GScaleSpinButton, self).__init__()        
        self.label = Gtk.Label.new(label)       
        self.content_widget = Gtk.SpinButton()
        self.delay = 0
        self.units = Gtk.Label.new(units)
        self._value_changed_timer = None

        self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, False, 2)
        self.pack_start(self.units, False, False, 2)
        
        #self.content_widget.set_editable(False)
        self.settings = Gio.Settings.new(schema)
        self.value = self.settings.get_uint(self.key)
        self.content_widget.set_range(self.min, self.max)
        self.content_widget.set_increments(step, page)

        home = os.path.expanduser("~")
        self.scale_file = os.path.join(home, ".config", "scale")

        if os.path.isfile(self.scale_file):
            with open(self.scale_file, 'r') as the_file:
                scale = the_file.read();
                self.content_widget.set_value (float(scale) * 100);

        self.content_widget.connect('value-changed', self.on_scale_value_changed)

    def on_scale_value_changed(self, widget):
        if self._value_changed_timer:
            GObject.source_remove(self._value_changed_timer)
        self._value_changed_timer = GObject.timeout_add(0, self.update_settings_value)

    def update_settings_value(self):
        value = self.content_widget.get_value()
      
        print(("Scale changed, save settings and filesi "+str(value)))
        with open(self.scale_file, 'w') as the_file:
            the_file.write(str(float(value)/100))
        self._value_changed_timer = None

        os.system("notify-send 缩放设置 您已设置缩放，注销登录生效")
        return False
    

class GSettingsSpinButton(Gtk.HBox):    
    def __init__(self, label, schema, key, dep_key, min, max, step, page, units):
        self.key = key
        self.min = min
        self.max = max
        self.dep_key = dep_key
        super(GSettingsSpinButton, self).__init__()        
        self.label = Gtk.Label.new(label)       
        self.content_widget = Gtk.SpinButton()
        self.delay = 0

        if units != "dddelay":
            self.units = Gtk.Label.new(units)
        else:
            self.delay = 1
        self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, False, 2)
        if (units != "" and self.delay == 0):
            self.pack_start(self.units, False, False, 2)
        
        #self.content_widget.set_editable(False)
        self.settings = Gio.Settings.new(schema)
        range = self.settings.get_range(self.key)
        if range[0] == "range":
            rangeDefault = (1 << 32) - 1
            rangeMin = rangeDefault
            rangeMax = rangeDefault
            range = range[1]
            rangeMin = range[0] if range[0] < rangeDefault else rangeDefault
            rangeMax = range[1] if range[1] < rangeDefault else rangeDefault
            self.min = min if min > rangeMin else rangeMin
            self.max = max if max < rangeMax else rangeMax
        self.content_widget.set_range(self.min, self.max)
        self.content_widget.set_increments(step, page)
        self.content_widget.set_value(self.settings.get_int(self.key))
        self.settings.connect("changed::"+self.key, self.on_my_setting_changed)
        self.content_widget.connect('value-changed', self.on_my_value_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)
        self._value_changed_timer = None

    def on_my_setting_changed(self, settings, key):
        self.content_widget.set_value(self.settings.get_int(self.key))

    def on_my_value_changed(self, widget):
        if self._value_changed_timer:
            GObject.source_remove(self._value_changed_timer)
        self._value_changed_timer = GObject.timeout_add(0, self.update_settings_value)
    
    def update_settings_value(self):
        if self.delay:
            time.sleep(1)
        self.settings.set_int(self.key, self.content_widget.get_value())
        self._value_changed_timer = None
        return False

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsEntry(Gtk.HBox):    
    def __init__(self, label, schema, key, dep_key):
        self.entryflag = False
        self.key = key
        self.dep_key = dep_key
        super(GSettingsEntry, self).__init__()
        self.label = Gtk.Label.new(label)       
        self.content_widget = Gtk.Entry()
        self.pack_start(self.label, False, False, 5)        
        self.add(self.content_widget)     
        self.settings = Gio.Settings.new(schema)        
        self.content_widget.set_text(self.settings.get_string(self.key))
        #self.settings.connect("changed::"+self.key, self.on_my_setting_changed)
        #self.content_widget.connect('focus-out-event', self.on_my_value_changed)     
        self.content_widget.connect('changed', self.on_content_value_changed) 
        self.content_widget.show_all()
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    #def on_my_setting_changed(self, settings, key):
    #    self.content_widget.set_text(self.settings.get_string(self.key))

    #def on_my_value_changed(self, event, widget):        
    #    self.settings.set_string(self.key, self.content_widget.get_text())

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

    def on_content_value_changed(self, *args, **kwargs):    
        if self.entryflag:
            if len(self.content_widget.get_text()) <= 32: 
                self.settings.set_string(self.key, self.content_widget.get_text())
            else:
                str1 = self.content_widget.get_text()
                self.content_widget.set_text(str1[0:32])    
                self.entryask(_("please input text len less than 32"))
        else:
            self.settings.set_string(self.key, self.content_widget.get_text())
    
    def entryask(self,msg):
        dialog = Gtk.MessageDialog(self.get_toplevel(),
                               Gtk.DialogFlags.DESTROY_WITH_PARENT,
                               Gtk.MessageType.ERROR,
                               Gtk.ButtonsType.OK,
                               None)
        dialog.set_default_size(200, 75)
        dialog.set_markup(msg)
        dialog.show_all()
        response = dialog.run()
        dialog.destroy()
        return response == Gtk.ResponseType.OK


class GSettingsFileChooser(Gtk.HBox):
    def __init__(self, label, schema, key, dep_key, show_none_cb = False):
        self.key = key
        self.dep_key = dep_key
        self.show_none_cb = show_none_cb
        super(GSettingsFileChooser, self).__init__()
        self.label = Gtk.Label.new(label)       
        self.content_widget = Gtk.FileChooserButton()
        self.pack_start(self.label, False, False, 2)
        self.add(self.content_widget)
        self.settings = Gio.Settings.new(schema)
        value = self.settings.get_string(self.key)     
        if value != "":
            self.content_widget.set_filename(value)

        if self.show_none_cb:
            self.show_none_cb_widget = Gtk.CheckButton(_("None"))
            self.show_none_cb_widget.set_active(value=="")
            self.pack_start(self.show_none_cb_widget, False, False, 5)        
            if value=="":
                self.content_widget.set_sensitive(False)
        
        self.content_widget.connect('file-set', self.on_my_value_changed)
        if self.show_none_cb:
            self.show_none_cb_widget.connect('toggled', self.on_my_value_changed)
        self.content_widget.show_all()
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_value_changed(self, widget):
        if self.show_none_cb:
            if self.show_none_cb_widget.get_active():
                value = ""
                self.content_widget.set_sensitive(False)
            else:
                value = self.content_widget.get_filename()
                if value==None:
                    value = ""
                self.content_widget.set_sensitive(True)
        else:
            value = self.content_widget.get_filename()
            if value==None:
                value = ""
        self.settings.set_string(self.key, value)

    def on_dependency_setting_changed(self, settings, dep_key):        
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsFontButton(Gtk.HBox):
    def __init__(self, label, schema, key, dep_key):
        self.key = key
        self.dep_key = dep_key
        super(GSettingsFontButton, self).__init__()

        #writelist = [_("WenQuanYi Micro Hei Mono"), _("WenQuanYi Zen Hei Mono"), _("WenQuanYi Micro Hei"),
        #             _("WenQuanYi Zen Hei"), _("WenQuanYi Zen Hei Sharp"), "Serif", "Sans", "AR", _("Source Han Sans CN")]
        writelist = []
        writeliststr = ""
        self.writere = None
        if 0 < len(writelist):
            writeliststr = "^"
            for item in writelist:
                if 0 > writeliststr.find(item):
                    writeliststr += item
                    if item != writelist[-1]:
                        writeliststr += "|"
            writeliststr += ".*"
            self.writere = re.compile(writeliststr);
        self.settings = Gio.Settings.new(schema)
        self.value = gettext.gettext(self.settings.get_string(key))
        
        self.label = Gtk.Label.new(label)

        self.content_widget = Gtk.FontButton()
        self.content_widget.set_font_name(self.value)
        
        if (label != ""):
            self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, False, 2)
        self.content_widget.connect('font-set', self.on_my_value_changed)
        self.content_widget.connect('button-press-event', self.on_font_button_pressed)
        self.content_widget.show_all()
        self.settings.connect("changed::"+self.key, self.on_my_settings_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_settings_changed(self, settings, key):
        self.value = gettext.gettext(self.settings.get_string(key))
        self.content_widget.set_font_name(self.value)

    def on_my_value_changed(self, widget):
        font_name = widget.get_font_name()
        split = font_name.split(' ');
        temp_value = split[len(split)-1]
        size=int(round(float(temp_value)))
        if (size > 5 and size < 73):
            self.value = font_name
            self.settings.set_string(self.key, font_name)
        else:
            self.content_widget.set_font_name(self.value)
    def on_font_button_pressed(self, widget, event):
        self.content_widget.set_filter_func(self.on_font_filter)
        return False;

    def on_font_filter(self, family, face):
        if re.search('Italic', face.get_face_name()):
            return False

        if None != self.writere:
            return (None != self.writere.match(family.get_name()));
        else:
            return True;

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

#class GSettingsRange(Gtk.HBox):
#    def __init__(self, label, schema, key, dep_key, **options):
#        self.key = key
#        self.dep_key = dep_key
#        super(GSettingsRange, self).__init__()
#        self.settings = Gio.Settings.new(schema)
#        self.value = self.settings.get_double(self.key)
#        
#        self.label = Gtk.Label.new(label)
#
#        #returned variant is range:(min, max)
#        _min, _max = self.settings.get_range(self.key)[1]
#
#        self.content_widget = Gtk.HScale.new_with_range(_min, _max, options.get('adjustment_step', 1))
#        self.content_widget.set_value(self.value)
#        if (label != ""):
#            self.pack_start(self.label, False, False, 2)
#        self.pack_start(self.content_widget, True, True, 2)
#        self.content_widget.connect('value-changed', self.on_my_value_changed)
#        self.content_widget.show_all()
#        self.dependency_invert = False
#        if self.dep_key is not None:
#            if self.dep_key[0] == '!':
#                self.dependency_invert = True
#                self.dep_key = self.dep_key[1:]
#            split = self.dep_key.split('/')
#            self.dep_settings = Gio.Settings.new(split[0])
#            self.dep_key = split[1]
#            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
#            self.on_dependency_setting_changed(self, None)
#
#    def on_my_value_changed(self, widget):
#        self.settings.set_double(self.key, widget.get_value())
#
#    def on_dependency_setting_changed(self, settings, dep_key):
#        if not self.dependency_invert:
#            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
#        else:
#            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsRange(Gtk.HBox):
    def __init__(self, label, low_label, hi_label, low_limit, hi_limit, inverted, valtype, exponential, schema, key, dep_key, **options):
        super(GSettingsRange, self).__init__()
        self.key = key
        self.dep_key = dep_key
        self.settings = Gio.Settings.new(schema)
        self.valtype = valtype

        if self.valtype == "int":
            self.value = self.settings.get_int(self.key) * 1.0
        elif self.valtype == "uint":
            self.value = self.settings.get_uint(self.key) * 1.0
        elif self.valtype == "double":
            self.value = self.settings.get_double(self.key) * 1.0
        self.label = Gtk.Label.new(label)
        self.label.set_alignment(1.0, 0.5)
        self.label.set_size_request(150, -1)
        self.low_label = Gtk.Label()
        self.low_label.set_alignment(0.5, 0.5)
        self.low_label.set_size_request(60, -1)
        self.hi_label = Gtk.Label()
        self.hi_label.set_alignment(0.5, 0.5)
        self.hi_label.set_size_request(60, -1)
        self.low_label.set_markup("<i><small>%s</small></i>" % low_label)
        self.hi_label.set_markup("<i><small>%s</small></i>" % hi_label)
        self.inverted = inverted
        self.exponential = exponential
        self._range = (hi_limit - low_limit) * 1.0
        self._step = options.get('adjustment_step', 1)
        self._min = low_limit * 1.0
        self._max = hi_limit * 1.0
        self.content_widget = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, -1 if self._min<0 else 0, 1, (self._step / self._range))
        self.content_widget.set_size_request(300, 0)
        self.content_widget.set_value(self.to_corrected(self.value))
        self.content_widget.set_draw_value(False);

        self.grid = Gtk.Grid()
        if (label != ""):
            self.grid.attach(self.label, 0, 0, 1, 1)
        if (low_label != ""):
            self.grid.attach(self.low_label, 1, 0, 1, 1)
        self.grid.attach(self.content_widget, 2, 0, 1, 1)
        if (hi_label != ""):
            self.grid.attach(self.hi_label, 3, 0, 1, 1)
        self.pack_start(self.grid, True, True, 2)
        self._dragging = False
        self.content_widget.connect('value-changed', self.on_my_value_changed)
        self.content_widget.connect('button-press-event', self.on_mouse_down)
        self.content_widget.connect('button-release-event', self.on_mouse_up)
        self.content_widget.connect("scroll-event", self.on_mouse_scroll_event)
        self.content_widget.show_all()
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

# halt writing gsettings during dragging
# it can take a long time to process all
# those updates, and the system can crash

    def on_mouse_down(self, widget, event):
        self._dragging = True

    def on_mouse_up(self, widget, event):
        self._dragging = False
        self.on_my_value_changed(widget)

    def on_mouse_scroll_event(self, widget, event):
        found, delta_x, delta_y = event.get_scroll_deltas()
        if found:
            add = delta_y < 0
            uncorrected = self.from_corrected(widget.get_value())
            if add:
                corrected = self.to_corrected(uncorrected + self._step)
            else:
                corrected = self.to_corrected(uncorrected - self._step)
            widget.set_value(corrected)
        return True

    def on_my_value_changed(self, widget):
        if self._dragging:
            return
        corrected = self.from_corrected(widget.get_value())
        if self.valtype == "int":
            self.settings.set_int(self.key, corrected)
        elif self.valtype == "uint":
            self.settings.set_uint(self.key, corrected)
        elif self.valtype == "double":
            self.settings.set_double(self.key, corrected)

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

    def to_corrected(self, value):
        result = 0.0
        if self.exponential:
            k = (math.log(self._max) - math.log(self._min)) / (self._range / self._step)
            a = self._max / math.exp(k * self._range)
            cur_val_step = (1 / (k / math.log(value / a))) / self._range
            if self.inverted:
                result = 1 - cur_val_step
            else:
                result = cur_val_step
        else:
            if self.inverted:
                result = 1 - ((value - self._min) / self._range)
            else:
                result = (value - self._min) / self._range
        return result

    def from_corrected(self, value):
        result = 0.0
        if self.exponential:
            k = (math.log(self._max)-math.log(self._min))/(self._range / self._step)
            a = self._max / math.exp(k * self._range)
            if self.inverted:
                cur_val_step = (1 - value) * self._range
                result = a * math.exp(k * cur_val_step)
            else:
                cur_val_step = value * self._range
                result =  a * math.exp(k * cur_val_step)
        else:
            if self.inverted:
                result = ((1 - value) * self._range) + self._min
            else:
                result =  (value * self._range) + self._min
        return round(result)

    def add_mark(self, value, position, markup):
        self.content_widget.add_mark((value - self._min) / self._range, position, markup)

class GSettingsRangeSpin(Gtk.HBox):
    def __init__(self, label, schema, key, dep_key, **options):
        self.key = key
        self.dep_key = dep_key
        super(GSettingsRangeSpin, self).__init__()
        self.label = Gtk.Label.new(label)
        self.content_widget = Gtk.SpinButton()

        if (label != ""):
            self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, False, 2)

        self.settings = Gio.Settings.new(schema)

        _min, _max = self.settings.get_range(self.key)[1]
        _increment = options.get('adjustment_step', 1)

        self.content_widget.set_range(_min, _max)
        self.content_widget.set_increments(_increment, _increment)
        #self.content_widget.set_editable(False)
        self.content_widget.set_numeric(True)
        self.content_widget.set_update_policy(Gtk.SpinButtonUpdatePolicy.IF_VALID)
        self.content_widget.set_digits(2)
        self.content_widget.set_value(self.settings.get_double(self.key))

        self.settings.connect("changed::"+self.key, self.on_my_setting_changed)
        self.content_widget.connect('value-changed', self.on_my_value_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_setting_changed(self, settings, key):
        value = self.settings.get_double(self.key)
        if value != self.content_widget.get_value():
            self.content_widget.set_value(value)

    def on_my_value_changed(self, widget):
        self.settings.set_double(self.key, self.content_widget.get_value())

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsComboBox(Gtk.HBox):    
    def __init__(self, label, schema, key, dep_key, options):
        self.key = key
        self.dep_key = dep_key
        super(GSettingsComboBox, self).__init__()
        self.settings = Gio.Settings.new(schema)        
        self.value = self.settings.get_string(self.key)
                      
        self.label = Gtk.Label.new(label)       
        self.model = Gtk.ListStore(str, str)
        selected = None
        for option in options:
            iter = self.model.insert_before(None, None)
            self.model.set_value(iter, 0, option[0])                
            self.model.set_value(iter, 1, option[1])                        
            if (option[0] == self.value):
                selected = iter
                                
        self.content_widget = Gtk.ComboBox.new_with_model(self.model)
        self.content_widget.set_wrap_width(1)   
        renderer_text = Gtk.CellRendererText()
        self.content_widget.pack_start(renderer_text, True)
        self.content_widget.add_attribute(renderer_text, "text", 1)     
        
        if selected is not None:
            self.content_widget.set_active_iter(selected)
        
        if (label != ""):
            self.pack_start(self.label, False, False, 2)                
        self.pack_start(self.content_widget, True, True, 2)                     
        self.content_widget.connect('changed', self.on_my_value_changed)
        self.content_widget.show_all()
        self.settings.connect("changed::"+self.key, self.on_my_setting_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_setting_changed(self, settings, key):
        value = self.settings.get_string(self.key)
        selected = None
        iter = self.model.get_iter_first()
        while True:
            option = self.model[iter][0]
            if (option == value):
                selected = iter
                break
            iter = self.model.iter_next(iter)
                                
        if selected is not None:
            self.content_widget.set_active_iter(selected)

    def on_my_value_changed(self, widget):
        tree_iter = widget.get_active_iter()
        if tree_iter != None:            
            value = self.model[tree_iter][0]            
            self.settings.set_string(self.key, value)

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsIntComboBox(Gtk.HBox):
    def __init__(self, label, schema, key, dep_key, options, use_uint=False):
        self.key = key
        self.dep_key = dep_key
        self.use_uint = use_uint
        super(GSettingsIntComboBox, self).__init__()
        self.settings = Gio.Settings.new(schema)
        if self.use_uint:
            self.value = self.settings.get_uint(self.key)
        else:
            self.value = self.settings.get_int(self.key)

        self.label = Gtk.Label.new(label)
        self.model = Gtk.ListStore(int, str)
        selected = None
        for option in options:
            iter = self.model.insert_before(None, None)
            self.model.set_value(iter, 0, option[0])
            self.model.set_value(iter, 1, option[1])
            if (option[0] == self.value):
                selected = iter

        self.content_widget = Gtk.ComboBox.new_with_model(self.model)
        renderer_text = Gtk.CellRendererText()
        self.content_widget.pack_start(renderer_text, True)
        self.content_widget.add_attribute(renderer_text, "text", 1)

        if selected is not None:
            self.content_widget.set_active_iter(selected)

        if (label != ""):
            self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, True, 2)
        self.content_widget.connect('changed', self.on_my_value_changed)
        self.content_widget.show_all()
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def on_my_value_changed(self, widget):
        tree_iter = widget.get_active_iter()
        if tree_iter != None:
            value = self.model[tree_iter][0]
            if self.use_uint:
                self.settings.set_uint(self.key, value)
            else:
                self.settings.set_int(self.key, value)

    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

class GSettingsSpecialVersionComboBox(Gtk.HBox):
    def __init__(self, label, schema, key, is_secadm, options, use_uint=False):
        self.key = key
        self.is_secadm = is_secadm
        self.use_uint = use_uint
        super(GSettingsSpecialVersionComboBox, self).__init__()
        self.settings = Gio.Settings.new(schema)
        if self.use_uint:
            self.value = self.settings.get_uint(self.key)
        else:
            self.value = self.settings.get_int(self.key)

        self.label = Gtk.Label.new(label)
        self.model = Gtk.ListStore(int, str)
        selected = None
        for option in options:
            iter = self.model.insert_before(None, None)
            self.model.set_value(iter, 0, option[0])
            self.model.set_value(iter, 1, option[1])
            if (option[0] == self.value):
                selected = iter

        self.content_widget = Gtk.ComboBox.new_with_model(self.model)
        renderer_text = Gtk.CellRendererText()
        self.content_widget.pack_start(renderer_text, True)
        self.content_widget.add_attribute(renderer_text, "text", 1)

        if selected is not None:
            self.content_widget.set_active_iter(selected)

        if (label != ""):
            self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, True, 2)
        self.content_widget.connect('changed', self.on_my_value_changed)
        self.content_widget.show_all()
        self.dependency_invert = False
        self.set_sensitive(self.is_secadm)


    def on_my_value_changed(self, widget):
        tree_iter = widget.get_active_iter()
        if tree_iter != None:
            value = self.model[tree_iter][0]
            if self.use_uint:
                self.settings.set_uint(self.key, value)
            else:
                self.settings.set_int(self.key, value)

            #timeoutstr="timeout: %d:%d:%d" %(0, value/60, 0)
            #with open('/opt/cdos-screensaver/cdos-screensaver.config', 'w+') as the_file:
            #    the_file.write(timeoutstr)

class GSettingsUIntComboBox(Gtk.HBox):
    def __init__(self, label, schema, key, options):
        self.key = key
        super(GSettingsUIntComboBox, self).__init__()
        self.settings = Gio.Settings.new(schema)
        self.value = self.settings.get_uint(self.key)

        self.label = Gtk.Label.new(label)
        self.model = Gtk.ListStore(int, str)
        selected = None
        for option in options:
            iter = self.model.insert_before(None, None)
            self.model.set_value(iter, 0, option[0])
            self.model.set_value(iter, 1, option[1])
            if (option[0] == self.value):
                selected = iter

        self.content_widget = Gtk.ComboBox.new_with_model(self.model)
        renderer_text = Gtk.CellRendererText()
        self.content_widget.pack_start(renderer_text, True)
        self.content_widget.add_attribute(renderer_text, "text", 1)

        if selected is not None:
            self.content_widget.set_active_iter(selected)

        if (label != ""):
            self.pack_start(self.label, False, False, 2)
        self.pack_start(self.content_widget, False, True, 2)
        self.content_widget.connect('changed', self.on_my_value_changed)
        self.content_widget.show_all()

    def on_my_value_changed(self, widget):
        tree_iter = widget.get_active_iter()
        if tree_iter != None:
            value = self.model[tree_iter][0]
            self.settings.set_uint(self.key, value)
        os.system('cdos --replace &')

class GSettingsColorChooser(Gtk.ColorButton):
    def __init__(self, schema, key, dep_key):
        Gtk.ColorButton.__init__(self)
        self._schema = Gio.Settings.new(schema)
        self._key = key
        self.dep_key = dep_key
        self.set_value(self._schema[self._key])
        self.connect("color-set", self._on_color_set)
#        self._schema.connect("changed::"+key, self._on_settings_value_changed)
        self.dependency_invert = False
        if self.dep_key is not None:
            if self.dep_key[0] == '!':
                self.dependency_invert = True
                self.dep_key = self.dep_key[1:]
            split = self.dep_key.split('/')
            self.dep_settings = Gio.Settings.new(split[0])
            self.dep_key = split[1]
            self.dep_settings.connect("changed::"+self.dep_key, self.on_dependency_setting_changed)
            self.on_dependency_setting_changed(self, None)

    def _on_settings_value_changed(self, schema, key):
        self.set_value(schema[key])
    def _on_color_set(self, *args):
        self._schema.set_string(self._key, self.get_value())
    def get_value(self):
        return self.get_rgba().to_string()
    def set_value(self, value):
        #self.set_rgba(Gdk.rgba_parse(value))
        rgba = Gdk.RGBA()
        rgba.parse(value)
        self.set_rgba(rgba)
    def on_dependency_setting_changed(self, settings, dep_key):
        if not self.dependency_invert:
            self.set_sensitive(self.dep_settings.get_boolean(self.dep_key))
        else:
            self.set_sensitive(not self.dep_settings.get_boolean(self.dep_key))

# class GConfFontButton(Gtk.HBox):
#     def __init__(self, label, key):
#         self.key = key
#         super(GConfFontButton, self).__init__()
#         self.settings = gconf.client_get_default()
#         self.value = self.settings.get_string(key)
        
#         self.label = Gtk.Label.new(label)

#         self.content_widget = Gtk.FontButton()
#         self.content_widget.set_font_name(self.value)
        
#         if (label != ""):
#             self.pack_start(self.label, False, False, 2)
#         self.pack_start(self.content_widget, False, False, 2)
#         self.content_widget.connect('font-set', self.on_my_value_changed)
#         self.content_widget.show_all()
#     def on_my_value_changed(self, widget):
#         self.settings.set_string(self.key, widget.get_font_name())

# class GConfComboBox(Gtk.HBox):    
#     def __init__(self, label, key, options, init_value = ""):  
#         self.key = key
#         super(GConfComboBox, self).__init__()
#         self.settings = gconf.client_get_default()  
#         self.value = self.settings.get_string(self.key)
#         if not self.value:
#             self.value = init_value
                      
#         self.label = Gtk.Label.new(label)       
#         self.model = Gtk.ListStore(str, str)
#         selected = None
#         for option in options:
#             iter = self.model.insert_before(None, None)
#             self.model.set_value(iter, 0, option[0])                
#             self.model.set_value(iter, 1, option[1])                        
#             if (option[0] == self.value):
#                 selected = iter
                                
#         self.content_widget = Gtk.ComboBox.new_with_model(self.model)   
#         renderer_text = Gtk.CellRendererText()
#         self.content_widget.pack_start(renderer_text, True)
#         self.content_widget.add_attribute(renderer_text, "text", 1)     
        
#         if selected is not None:
#             self.content_widget.set_active_iter(selected)
        
#         if (label != ""):
#             self.pack_start(self.label, False, False, 2)                
#         self.pack_start(self.content_widget, False, False, 2)                     
#         self.content_widget.connect('changed', self.on_my_value_changed)
#         # The on_my_setting_changed callback raises a segmentation fault, need to investigate that
#         #self.settings.add_dir(os.path.split(key)[0], gconf.CLIENT_PRELOAD_NONE)
#         #self.settings.notify_add(self.key, self.on_my_setting_changed)
#         self.content_widget.show_all()
        
#     def on_my_value_changed(self, widget):
#         tree_iter = widget.get_active_iter()
#         if tree_iter != None:            
#             value = self.model[tree_iter][0]            
#             self.settings.set_string(self.key, value)
#     def on_my_setting_changed(self, client, cnxn_id, entry, args):
#         print entry

# class GConfCheckButton(Gtk.CheckButton):    
#     def __init__(self, label, key):        
#         self.key = key
#         super(GConfCheckButton, self).__init__(label)       
#         self.settings = gconf.client_get_default()
#         self.set_active(self.settings.get_bool(self.key))
#         self.settings.notify_add(self.key, self.on_my_setting_changed)
#         self.connect('toggled', self.on_my_value_changed)            
    
#     def on_my_setting_changed(self, client, cnxn_id, entry):
#         value = entry.value.get_bool()
#         self.set_active(value)
        
#     def on_my_value_changed(self, widget):
#         self.settings.set_bool(self.key, self.get_active())

class DBusCheckButton(Gtk.CheckButton):    
    def __init__(self, label, service, path, get_method, set_method):        
        super(DBusCheckButton, self).__init__(label)     
        proxy = dbus.SystemBus().get_object(service, path)
        self.dbus_iface = dbus.Interface(proxy, dbus_interface=service)
        self.dbus_get_method = get_method
        self.dbus_set_method = set_method
        self.set_active(getattr(self.dbus_iface, get_method)()[1])
        self.connect('toggled', self.on_my_value_changed)
        
    def on_my_value_changed(self, widget):
        getattr(self.dbus_iface, self.dbus_set_method)(self.get_active())
