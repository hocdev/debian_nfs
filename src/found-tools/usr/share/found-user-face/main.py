#!/usr/bin/python3
import sys
import os
import grp
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('AccountsService', '1.0')
from gi.repository import Gtk, GdkPixbuf, AccountsService, Gdk, Wnck
import gettext
import getpass
import shutil
import PIL
import configparser  # niyl
from PIL import Image
import fcntl

import foundtools.config as ftconfig

gettext.bindtextdomain(ftconfig.GETTEXT_PACKAGE, ftconfig.localedir)
gettext.textdomain(ftconfig.GETTEXT_PACKAGE)
_ = gettext.gettext

DEFAULT_AVATAR = "/usr/share/faces/user-generic.png"
AVATAR_DIR = "/usr/share/found-user-face"
UI_FILE = os.path.join(AVATAR_DIR, "window.ui")
CSS_FILE = os.path.join(AVATAR_DIR, "style.css")
PREVIEW_SIZE = 96
ITEM_SIZE = 72

class Lock(object):
    def __init__(self, filename):
        self.handle = open(filename, 'r')

    def acquire(self):
        fcntl.flock(self.handle, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def release(self):
        fcntl.flock(self.handle, fcntl.LOCK_UN)

    def __del__(self):
        self.handle.close()


class AvatorDialog(object):
    def __init__(self):
        try:
            self.lock_filename = os.path.realpath(__file__)
            self.lock = Lock(self.lock_filename)
            self.lock.acquire()
        except Exception as e:
            Wnck.Screen.get_default().force_update()
            a = Wnck.Screen.get_default()
            for item in a.get_windows():
                if item.get_name() == _("Avatar Switch"):
                    item.activate(0)
                    print("AvatarSwitch: has one instance running")
                    sys.exit()
        try:
            self.set_theme()
            self.builder = Gtk.Builder()
            self.builder.set_translation_domain(ftconfig.GETTEXT_PACKAGE)
            self.builder.add_from_file(UI_FILE)
            self.main_window = self.builder.get_object('main_window')
            self.path = ""
            self.default_pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(
                DEFAULT_AVATAR, PREVIEW_SIZE, PREVIEW_SIZE)
            self.rectangle_avatar = self.builder.get_object('rectangle_avatar')
            self.rectangle_avatar.set_from_pixbuf(self.default_pixbuf)

            user_manager = AccountsService.UserManager
            current_user = getpass.getuser()
            self.account_service = user_manager.get_default().get_user(current_user)
            self.account_service.connect('notify::is-loaded',
                                         self.load_user_info)
            self.scroll_window = self.builder.get_object('avatar_sw')
            custom_button = self.builder.get_object('custom_avatar_button')
            custom_button.connect('clicked',
                                  self._on_face_browse_menuitem_activated)
            self.grid = Gtk.Grid()
            self.grid.set_name('items_grid')
            self.grid.set_row_spacing(12)
            self.grid.set_column_spacing(12)
            self.load_system_avatars()

            self.scroll_window.add(self.grid)
            self.cancel_button = self.builder.get_object('cancel_button')
            self.ok_button = self.builder.get_object('ok_button')
            self.cancel_button.connect("clicked", self.on_cancel_button)
            self.ok_button.connect("clicked", self.on_ok_button)
            self.main_window.connect("destroy", Gtk.main_quit)
            self.main_window.show_all()
        except Exception as detail:
            print(("AvatarSwitch: Init error, %s" % detail))

    def load_system_avatars(self):
        row = 0
        col = 0
        num_cols = 5
        face_dirs = ["/usr/share/faces"]
        for face_dir in face_dirs:
            if os.path.exists(face_dir):
                pictures = sorted(os.listdir(face_dir))
                for picture in pictures:
                    path = os.path.join(face_dir, picture)
                    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(
                        path, ITEM_SIZE, ITEM_SIZE)
                    image = Gtk.Image.new_from_pixbuf(pixbuf)
                    butitem = Gtk.Button()
                    butitem.set_image(image)
                    butitem.path = path
                    butitem.connect("clicked", self.on_face_menuitem_activated)
                    self.grid.attach(butitem, col, row, 1, 1)
                    #self.grid.attach(self.grid , butitem , row , col , 1 , 1)
                    col = (col + 1) % num_cols
                    if col == 0:
                        row = row + 1

    def update_avatar_preview_and_pixbuf(self, path):
        try:
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(
                path, PREVIEW_SIZE, PREVIEW_SIZE)
            self.rectangle_avatar.set_from_pixbuf(pixbuf)
        except:
            print("AvatarSwitch: Cannot set avatar, reset to default.")
            self.rectangle_avatar.set_from_pixbuf(self.default_pixbuf)

    def on_face_menuitem_activated(self, widget):
        path = widget.path
        if os.path.exists(path):
            self.account_service.set_icon_file(path)
            self.update_avatar_preview_and_pixbuf(path)
            self.path = path
            shutil.copy(path, self.account_service.get_home_dir() + "/.face")
        else:
            self.path = ""

    def on_cancel_button(self, button):
        print("AvatarSwitch: Cancel clicked")
        Gtk.main_quit()

    def on_ok_button(self, button):
        print(("AvatarSwitch: Ok clicked, %s" % self.path))
        if os.path.exists(self.path):
            self.account_service.set_icon_file(self.path)
            #self.path = path
        Gtk.main_quit()

    def load_user_info(self, user, param):
        #self.realname_entry.set_text(user.get_real_name())
        path = user.get_icon_file()
        if os.path.exists(path):
            self.update_avatar_preview_and_pixbuf(path)

    def loaded_update_user_info(self):
        user = self.account_service
        path = user.get_icon_file()
        if os.path.exists(path):
            self.path = path
            self.update_avatar_preview_and_pixbuf(user.get_icon_file())

    def update_preview_cb(self, dialog, preview):
        filename = dialog.get_preview_filename()
        dialog.set_preview_widget_active(False)
        if filename and os.path.isfile(filename):
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(filename, 128, 128)
            if pixbuf is not None:
                preview.set_from_pixbuf(pixbuf)
                dialog.set_preview_widget_active(True)

    def _on_face_browse_menuitem_activated(self, widget):
        dialog = Gtk.FileChooserDialog(
            None, None, Gtk.FileChooserAction.OPEN,
            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OPEN,
             Gtk.ResponseType.OK))
        dialog.set_current_folder(self.account_service.get_home_dir())
        print(("AvatarSwitch: Name is %s" %
              (self.account_service.get_home_dir())))
        type_filter = Gtk.FileFilter()
        type_filter.set_name(_("Images"))
        type_filter.add_mime_type("image/*")
        dialog.add_filter(type_filter)
        dialog.set_title(_("select pictures"))

        preview = Gtk.Image()
        dialog.set_preview_widget(preview)
        dialog.connect("update-preview", self.update_preview_cb, preview)
        dialog.set_use_preview_label(False)

        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            path = dialog.get_filename()
            image = PIL.Image.open(path)
            width, height = image.size
            if width > height:
                new_width = height
                new_height = height
            elif height > width:
                new_width = width
                new_height = width
            else:
                new_width = width
                new_height = height
            left = (width - new_width)/2
            top = (height - new_height)/2
            right = (width + new_width)/2
            bottom = (height + new_height)/2
            image = image.crop((left, top, right, bottom))
            image.thumbnail((96, 96), Image.LANCZOS)
            face_path = os.path.join(self.account_service.get_home_dir(), ".face")
            image.save(face_path, "png")
            self.account_service.set_icon_file(face_path)
            print(("AvatarSwitch: Image save path %s" % face_path))
            self.path = face_path
            self.update_avatar_preview_and_pixbuf(face_path)
        dialog.destroy()

    def set_theme(self):
        provider = Gtk.CssProvider()
        provider.load_from_path(CSS_FILE)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def run(self):
        Gtk.main()


if __name__ == "__main__":
    win = AvatorDialog()
    win.run()
