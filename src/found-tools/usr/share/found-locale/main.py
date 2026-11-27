#!/usr/bin/python3
# *-* coding: utf-8 *-*

#/** 
 #* Copyright (c) 2016-2018 CPU and Fundamental Software Research Center, CAS
 #* 
 #* Based on code from ./cinnamon-settings-locale/cinnamon-settings-locale.py in xorg-server. 
 #* Copyright (c) 1997-2003 by The XFree86 Project, Inc.
 #* 
 #* This software is published by the license of CPU-OS License, you can use and 
 #* distibuted this software under this License. See CPU-OS Liense for more detail.
 #*
 #* You should have receive a copy of CPU-OS License. If not, please contact us 
 #* by email <support_os@cpu-os.ac.cn>  
 #* 
 #**/


"""Change Language
Try change Linux system language
"""

import os
import gi
import gettext
import subprocess
from string import Template
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk

# i18n
import foundtools.config as ftconfig

gettext.bindtextdomain(ftconfig.GETTEXT_PACKAGE, ftconfig.localedir)
gettext.textdomain(ftconfig.GETTEXT_PACKAGE)
_ = gettext.gettext

LOCAL_FILE_PATH = "/etc/default/locale"
UI_FILE_PATH = "/usr/share/found-locale/window.ui"
CLASS_FILE_PATH = "/usr/share/found-locale/style.css"
locale_template = Template("LANG=${encoding}\n"
                           "LANGUAGE=${country}:${lang}\n"
                           "LC_CTYPE=${encoding}\n"
                           "LC_NUMERIC=${encoding}\n"
                           "LC_TIME=${encoding}\n"
                           "LC_COLLATE=${encoding}\n"
                           "LC_MONETARY=${encoding}\n"
                           "LC_MESSAGES=${encoding}\n"
                           "LC_PAPER=${encoding}\n"
                           "LC_NAME=${encoding}\n"
                           "LC_ADDRESS=${encoding}\n"
                           "LC_TELEPHONE=${encoding}\n"
                           "LC_MEASUREMENT=${encoding}\n"
                           "LC_IDENTIFICATION=${encoding}\n"
                           "LC_ALL=")


class AlertDialog(Gtk.MessageDialog):
    """
    Alert when throw exception.
    """

    def __init__(self, parent, message, info):
        """construct"""
        dialog = Gtk.MessageDialog(parent, 2, Gtk.MessageType.ERROR,
                                   Gtk.ButtonsType.CLOSE, message,
                                   title=_("Alert"))
        dialog.format_secondary_text(info)
        dialog.set_default_size(300, 100)
        dialog.show_all()
        dialog.run()
        dialog.destroy()


class LogoutDialog(Gtk.Dialog):
    """
    This class is for display general information and error messages.
    """

    def __init__(self, parent):
        Gtk.Dialog.__init__(self, _("Alert"), parent,
                            Gtk.DialogFlags.DESTROY_WITH_PARENT,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))
        self.set_default_size(400, 60)

        label = Gtk.Label(_("Changes will effets on next reboot, reboot now?"))
        box = self.get_content_area()
        box.pack_start(label, False, False, 0)
        self.show_all()
        response = self.run()

        if response == Gtk.ResponseType.OK:
            subprocess.call("reboot", shell=True)

        self.destroy()


class Module:
    def __init__(self):
        try:
            self.builder = Gtk.Builder()
            self.builder.add_from_file(UI_FILE_PATH)
            self.builder.set_translation_domain(ftconfig.GETTEXT_PACKAGE)
            self.window = self.builder.get_object("main_window")
            # add for app icon
            # self.window.set_wmclass("found-locale",
            #                         "found-locale")
            # add for translate
            self.window.set_title(_("Language Support"))
            self.window.connect("destroy", Gtk.main_quit)
            #self.set_theme(CLASS_FILE_PATH)
            self.lang_widget_construct()
            # add for translate
            lang_label_text = _("Language for menus and windows:")
            self.builder.get_object("lang_label").set_label(lang_label_text)
            # add for translate
            lang_tip_text = _("Changes take effect next time you reboot.")
            self.builder.get_object("lang_tip").set_label(lang_tip_text)
            apply_button = self.builder.get_object("apply_button")
            # add for translate
            apply_button.set_label(_("Apply System-Wide"))
            apply_button.connect("clicked", self.on_apply_button_clicked)
            # add for translate
            apply_label_text = _("Use the same language chioces "
                                 "for startup and the login screen.")
            self.builder.get_object("apply_label").set_label(apply_label_text)
            apply_tip_text = _("Will change some format, like numeric, "
                               "time, monetary, telephone, and more.")
            self.builder.get_object("apply_tip").set_label(apply_tip_text)
            self.window.show_all()
        except Exception as detail:
            print(detail)

    def set_theme(self, file_theme):
        screen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        provider.load_from_path(file_theme)
        context = Gtk.StyleContext()
        context.add_provider_for_screen(screen,
                                        provider,
                                        Gtk.STYLE_PROVIDER_PRIORITY_USER)

    def configured_locale(self):
        try:
            with open(LOCAL_FILE_PATH, encoding="utf-8") as config:
                for line in config:
                    name, _, value = line.partition("=")
                    if name.strip() == "LANG":
                        return value.strip().strip("'\"")
        except OSError:
            pass
        return os.environ.get("LANG") or "en_US.UTF-8"

    def lang_widget_construct(self):
        lang_store = Gtk.ListStore(str, str)
        lang_store.append([_("English"), "en_US.UTF-8"])
        lang_store.append([_("Vietnamese"), "vi_VN.UTF-8"])

        lang_combo = self.builder.get_object("lang_combo")
        lang_combo.set_model(lang_store)
        current_locale = self.configured_locale()
        language = current_locale.split(".")[0].replace("-", "_").split("_")[0]
        lang_combo.set_active(1 if language == "vi" else 0)

        renderer_text = Gtk.CellRendererText()
        lang_combo.pack_start(renderer_text, True)
        lang_combo.add_attribute(renderer_text, "text", 0)

    def on_apply_button_clicked(self, button):
        combo = self.builder.get_object("lang_combo")
        selected = combo.get_active_iter()
        if selected is not None:
            language = combo.get_model().get_value(selected, 1)
            if self.apply_to_system_wide(language):
                LogoutDialog(self.window)

    def apply_to_system_wide(self, lang_with_encoding, file=LOCAL_FILE_PATH):
        lang = lang_with_encoding[0:2]
        country = lang_with_encoding[0:5]
        result = locale_template.safe_substitute(encoding=lang_with_encoding,
                                                 country=country,
                                                 lang=lang)
        try:
            with open(file, "w", encoding="utf-8") as config:
                config.write(result)
            return True
        except Exception as e:
            AlertDialog(self.window,
                        message=_("Change language configuration failed!"),
                        info=str(e))
            print(e)
            return False


if __name__ == "__main__":
    module = Module()
    Gtk.main()
