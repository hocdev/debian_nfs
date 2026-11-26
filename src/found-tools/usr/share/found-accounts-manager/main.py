#!/usr/bin/python3
import sys
import os
import subprocess
import pwd
import grp
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('AccountsService', '1.0')
gi.require_version('Pango', '1.0')
from gi.repository import Gtk, GObject, Gio, GdkPixbuf, AccountsService, Pango
import gettext
import shutil
import re # liting
import PIL
import configparser # niyl
from PIL import Image
from random import randint
import pwquality #passwd
import configparser

import foundtools.config as ftconfig

gettext.bindtextdomain(ftconfig.GETTEXT_PACKAGE, ftconfig.localedir)
gettext.textdomain(ftconfig.GETTEXT_PACKAGE)
_ = gettext.gettext

(INDEX_USER_OBJECT, INDEX_USER_PICTURE, INDEX_USER_DESCRIPTION) = list(range(3))
(INDEX_GID, INDEX_GROUPNAME) = list(range(2))
LIGHTDM_CONFIG_FILE = "/etc/lightdm/lightdm.conf"


class GroupDialog(Gtk.Dialog):
    def __init__(self, label, value):
        super(GroupDialog, self).__init__()

        try:
            self.set_modal(True)
            self.set_skip_taskbar_hint(True)
            self.set_skip_pager_hint(True)
            # self.set_title(label)
            # modified by liting @ 2015-06-05
            self.set_title(label)
            # end by liting
            table = DimmedTable()
            table.add_labels([label])

            self.entry = Gtk.Entry()
            self.entry.set_text(value)
            self.entry.connect("changed", self._on_entry_changed)
            table.add_controls([self.entry])

            self.set_border_width(6)

            box = self.get_content_area()
            box.add(table)
            self.show_all()

            self.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK, )
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
        except Exception as detail:
            print(detail)

    def _on_entry_changed(self, entry):
        groupname = entry.get_text()
        print(groupname)
        # modified by liting @ 2015-06-01 for checking groupname
        zh_pattern = re.compile('[\\u4e00-\\u9fa5]+')

        if groupname[0] == "-" or groupname[0] == "+" or groupname[0] == "~":
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The group name cannot start with dash, plus or tilde."))
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
        elif " " in groupname or zh_pattern.search(groupname.encode('utf-8').decode('utf8')):
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The group name cannot contain space characters or chinese characters"))
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
        elif ":" in groupname or ";" in groupname or "," in groupname:
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The group name cannot contain colon, comma or semicolon."))
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
        else:
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, None)
            self.set_response_sensitive(Gtk.ResponseType.OK, True)

        if entry.get_text() == "":
            self.set_response_sensitive(Gtk.ResponseType.OK, False)

        namepat = re.compile(r'^[a-z_][a-z0-9_-]*[$]?')
        matcha = namepat.match(entry.get_text().lower())
        if not matcha:
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("Group name can not start with digits and can not contains invalid symbols."))
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
        # add by liting @ 2012-06-01 , fix bug for unlimited length
        if len(groupname) > 32:
            entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The length of group name is too long!"))
            self.set_response_sensitive(Gtk.ResponseType.OK, False)
            print((len(groupname)))
        # end by liting

        # end modified by liting


class DimmedTable(Gtk.Table):
    def __init__(self):
        super(DimmedTable, self).__init__()
        self.set_border_width(6)
        self.set_row_spacings(8)
        self.set_col_spacings(15)

    def add_labels(self, texts):
        row = 0
        for text in texts:
            if text != None:
                label = Gtk.Label(text)
                label.set_alignment(0.5, 0.1)
                label.get_style_context().add_class("dim-label")
                self.attach(label, 0, 2, row, row+1, xoptions=Gtk.AttachOptions.EXPAND|Gtk.AttachOptions.FILL)
            row = row + 1

    def add_controls(self, controls):
        row = 1
        for control in controls:
            self.attach(control, 1, 2, row, row+1)
            row = row + 1


class EditableEntry(Gtk.Notebook):

    __gsignals__ = {
        'changed': (GObject.SIGNAL_RUN_FIRST, None,
                    (str,))
    }

    PAGE_BUTTON = 0
    PAGE_ENTRY = 1

    def __init__(self):
        super(EditableEntry, self).__init__()

        self.label = Gtk.Label()
        self.entry = Gtk.Entry()
        self.button = Gtk.Button()

        self.button.set_alignment(0.0, 0.5)
        self.button.set_relief(Gtk.ReliefStyle.NONE)
        self.append_page(self.button, None)
        self.append_page(self.entry, None)
        self.set_current_page(0)
        self.set_show_tabs(False)
        self.set_show_border(False)
        self.editable = False
        self.show_all()

        self.button.connect("released", self._on_button_clicked)
        self.button.connect("activate", self._on_button_clicked)
        self.entry.connect("activate", self._on_entry_validated)
        self.entry.connect("changed", self._on_entry_changed)
        self.entry.connect("focus-out-event", self._on_entry_focus_out)

    def set_text(self, text):
        self.button.set_label(text)
        self.entry.set_text(text)

    def _on_button_clicked(self, button):
        self.set_editable(True)

    def _on_entry_validated(self, entry):
        self.set_editable(False)
        self.emit("changed", entry.get_text())

    def _on_entry_changed(self, entry):
        # add by liting @ 2012-05-29 , fix bug for unlimited length
        if len(entry.get_text()) > 32:
            self.set_editable(False)
        # end by liting
        self.button.set_label(entry.get_text())
        self.emit("changed", entry.get_text())

    # add by niyl
    def _on_entry_focus_out(self, entry, event):
        self.set_editable(False)

    def set_editable(self, editable):
        if editable:
            self.set_current_page(EditableEntry.PAGE_ENTRY)
        else:
            self.set_current_page(EditableEntry.PAGE_BUTTON)
        self.editable = editable

    def set_tooltip_text(self, tooltip):
        self.button.set_tooltip_text(tooltip)

    def get_editable(self):
        return self.editable

    def get_text(self):
        return self.entry.get_text()


class PasswordDialog(Gtk.Dialog):

    def __init__(self, user, password_mask, group_mask):
        super(PasswordDialog, self).__init__()

        self.user = user
        self.password_mask = password_mask
        self.group_mask = group_mask

        self.set_modal(True)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_title("")

        table = DimmedTable()
        table.add_labels([_("New password"), None, _("Confirm password")])

        self.new_password = Gtk.Entry()
        self.new_password.set_icon_from_icon_name(Gtk.EntryIconPosition.SECONDARY, "reload")
        self.new_password.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("Generate a password"))
        self.new_password.connect("icon-release", self._on_new_password_icon_released)
        self.new_password.connect("changed", self._on_passwords_changed)
        table.attach(self.new_password, 1, 3, 0, 1)

        self.strengh_indicator = Gtk.ProgressBar()
        self.strengh_indicator.set_tooltip_text(_("Your new password needs to be at least 8 characters long"))
        self.strengh_indicator.set_fraction(0.0)
        table.attach(self.strengh_indicator, 1, 2, 1, 2, xoptions=Gtk.AttachOptions.EXPAND | Gtk.AttachOptions.FILL)
        self.strengh_indicator.set_size_request(-1, 1)

        self.strengh_label = Gtk.Label()
        self.strengh_label.set_tooltip_text(_("Your new password needs to be at least 8 characters long"))
        self.strengh_label.set_alignment(1, 0.5)
        table.attach(self.strengh_label, 2, 3, 1, 2)

        self.confirm_password = Gtk.Entry()
        self.confirm_password.connect("changed", self._on_passwords_changed)
        table.attach(self.confirm_password, 1, 3, 2, 3)

        self.show_password = Gtk.CheckButton(_("Show password"))
        self.show_password.connect('toggled', self._on_show_password_toggled)
        table.attach(self.show_password, 1, 3, 3, 4)

        self.set_border_width(6)

        box = self.get_content_area()
        box.add(table)
        self.show_all()

        self.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, _("Change"), Gtk.ResponseType.OK, )

        self.set_passwords_visibility()
        self.set_response_sensitive(Gtk.ResponseType.OK, False)

        self.connect("response", self._on_response)

    def _on_response(self, dialog, response_id):
        if response_id == Gtk.ResponseType.OK:
            self.change_password()
        else:
            self.destroy()

    def change_password(self):
        newpass = self.new_password.get_text()
        username = self.user.get_user_name()
        #self.user.set_password(newpass, "")
        x = os.system("echo  \'%s\\n%s\' |passwd %s" %(newpass,newpass,username))
        if x != 0:
            self.strengh_label.set_text(_("Error"))
            return 0
        os.system("gpasswd -d '%s' nopasswdlogin" % self.user.get_user_name())
        mask = self.group_mask.get_text()
        mask = mask.split(", ")
        if 'nopasswdlogin' in mask:
            mask.remove("nopasswdlogin")
        mask = ", ".join(mask)
        self.group_mask.set_text(mask)
        self.password_mask.set_text('\\u2022\\u2022\\u2022\\u2022\\u2022\\u2022')
        self.destroy()

    def set_passwords_visibility(self):
        visible = self.show_password.get_active()
        self.new_password.set_visibility(visible)
        self.confirm_password.set_visibility(visible)

    def _on_new_password_icon_released(self, widget, icon_pos, event):
        self.show_password.set_active(True)
        characters = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-"
        newpass = ""
        for i in range(8):
            index = randint(0, len(characters) - 1)
            newpass = newpass + characters[index]

        self.new_password.set_text(newpass)
        self.confirm_password.set_text(newpass)
        self.check_passwords()

    def _on_show_password_toggled(self, widget):
        self.set_passwords_visibility()

    def _on_passwords_changed(self, widget):
        new_password = self.new_password.get_text()
        if self.has_cn(new_password):
            self.new_password.set_text("")
            self.entryask(_("can not input chinese character"))
        else:
            confirm_password = self.confirm_password.get_text()
            if new_password != confirm_password:
                self.confirm_password.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
                self.confirm_password.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("Passwords do not match"))
            else:
                self.confirm_password.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, None)
            if len(new_password) < 8:
                self.strengh_label.set_text(_("Too short"))
                self.strengh_indicator.set_fraction(0.0)
            else:
                self.strengh_label.set_text(_("OK"))
                self.strengh_indicator.set_fraction(1.0)

        self.check_passwords()

    def check_passwords(self):
        r = 0
        cf = configparser.ConfigParser()
        with open("/etc/security/pwquality.conf",'r') as f:
            con = f.read()
        con = "[config]\n"+con
        cf.read_string(str(con))
        settings = pwquality.PWQSettings()
        if cf.has_option("config","ucredit"):
            settings.set_option("ucredit=" + cf.get("config","ucredit"))
        else:
            settings.set_option("ucredit=0")
        if cf.has_option("config","dcredit"):
            settings.set_option("dcredit=" + cf.get("config","dcredit"))
        else:
            settings.set_option("dcredit=0")
        if cf.has_option("config","lcredit"):
            settings.set_option("lcredit=" + cf.get("config","lcredit"))
        else:
            settings.set_option("lcredit=0")
        if cf.has_option("config","ocredit"):
            settings.set_option("ocredit=" + cf.get("config","ocredit"))
        else:
            settings.set_option("ocredit=0")
        if cf.has_option("config","minclass"):
            settings.set_option("minclass=" + cf.get("config","minclass"))
        else:
            settings.set_option("minclass=0")
        if cf.has_option("config","minlen"):
            settings.set_option("minlen=" + cf.get("config","minlen"))
        else:
            settings.set_option("minlen=8")
        new_password = self.new_password.get_text()
        confirm_password = self.confirm_password.get_text()
        if len(new_password) >= 8 and new_password == confirm_password:
            if cf.has_option("config","ucredit") or cf.has_option("config","minclass") or cf.has_option("config","dcredit") or cf.has_option("config","lcredit") or cf.has_option("config","ocredit") or cf.has_option("config","minlen"):
                try:
                    r = settings.check(new_password,None) 
                except pwquality.PWQError as e:
                    error = e.args[1]
                if r != 0:
                    self.set_response_sensitive(Gtk.ResponseType.OK, True)
                else:
                    self.set_response_sensitive(Gtk.ResponseType.OK, False)
                    self.strengh_label.set_text(_("Easy"))
            else:
                self.set_response_sensitive(Gtk.ResponseType.OK, True)
        else:
            self.set_response_sensitive(Gtk.ResponseType.OK, False)

    def entryask(self, msg):
        dialog = Gtk.MessageDialog(None, Gtk.DialogFlags.DESTROY_WITH_PARENT, Gtk.MessageType.ERROR, Gtk.ButtonsType.OK, None)
        dialog.set_default_size(400, 200)
        dialog.set_markup(msg)
        dialog.show_all()
        response = dialog.run()
        dialog.destroy()
        return response == Gtk.ResponseType.OK

    def has_cn(self, string):
        flag = False
        string = string.decode('UTF-8')
        for ch in string:
            if ch >= '\\u4e00' and ch <= '\\u9fa5':
                flag = True
                break
            else:
                continue
        return flag


class NewUserDialog(Gtk.Dialog):
    def __init__(self):
        super(NewUserDialog, self).__init__()

        try:
            self.set_modal(True)
            self.set_skip_taskbar_hint(True)
            self.set_skip_pager_hint(True)
            # self.set_title("")
            # modified by liting @ 2015-06-05
            self.set_title(_("Username"))
            # end by liting
            self.account_type_combo = Gtk.ComboBoxText()
            self.account_type_combo.append_text(_("Standard"))
            self.account_type_combo.append_text(_("Administrator"))
            self.account_type_combo.set_active(0)

            self.account_type_notice = Gtk.Label("")
            notice_mark = "<span color=\"red\">{}</span>".format(_("Once confirmed, cannot be changed."))
            self.account_type_notice.set_markup(notice_mark)

            self.realname_entry = Gtk.Entry()
            self.realname_entry.connect("changed", self._on_info_changed)

            self.username_entry = Gtk.Entry()
            self.username_entry.connect("changed", self._on_info_changed)

            table = DimmedTable()
            table.add_labels([_("Account Type"), None, _("Full Name"), _("Username")])
            table.add_controls([self.account_type_combo, self.account_type_notice, self.realname_entry, self.username_entry])

            self.set_border_width(6)

            box = self.get_content_area()
            box.add(table)
            self.show_all()

            self.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_ADD, Gtk.ResponseType.OK, )
            self.set_response_sensitive(Gtk.ResponseType.OK, False)

        except Exception as detail:
            print(detail)

    def _on_info_changed(self, widget):
        """modified by liting

        import regular expression to CHECK chinese charachter
        """
        fullname = self.realname_entry.get_text()
        username = self.username_entry.get_text()
        valid = True
        zh_pattern = re.compile('[\\u4e00-\\u9fa5]+')

        if " " in username or zh_pattern.search(username.decode('utf8')):
            self.username_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            self.username_entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The username cannot contain space characters or chinese characters"))
            valid = False
        if len(username) > 32:
            self.username_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            self.username_entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The username cannot larger than 32"))
            valid = False
        else:
            self.username_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, None)

        if " " in fullname or zh_pattern.search(fullname.decode('utf8')):
            self.realname_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            self.realname_entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The username cannot contain space characters or chinese characters"))
            valid = False

        if len(fullname) > 32:
            self.realname_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, Gtk.STOCK_DIALOG_WARNING)
            self.realname_entry.set_icon_tooltip_text(Gtk.EntryIconPosition.SECONDARY, _("The username cannot larger than 32"))
            valid = False
        else:
            self.realname_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, None)
        # end liting
        if username == "" or fullname == "":
            valid = False

        self.set_response_sensitive(Gtk.ResponseType.OK, valid)


class GroupsDialog(Gtk.Dialog):

    def __init__(self, username):
        super(GroupsDialog, self).__init__()

        try:
            self.set_modal(True)
            self.set_skip_taskbar_hint(True)
            self.set_skip_pager_hint(True)
            self.set_title(username +" " + _("Groups belongs"))
            self.set_default_size(200, 480)

            scrolled = Gtk.ScrolledWindow()
            viewport = Gtk.Viewport()
            vbox = Gtk.VBox()
            self.checkboxes = []
            groups = sorted(grp.getgrall(), key=lambda x: x[0], reverse=False)
            for group in groups:
                checkbox = Gtk.CheckButton(group[0])
                self.checkboxes.append(checkbox)
                vbox.add(checkbox)
                if username in group[3]:
                    checkbox.set_active(True)
                if group[0] == 'root':
                    checkbox.set_sensitive(False)
            viewport.add(vbox)
            scrolled.add(viewport)
            self.set_border_width(6)

            box = self.get_content_area()
            box.pack_start(scrolled, True, True, 0)
            self.show_all()

            self.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK, )

        except Exception as detail:
            print(detail)

    def get_selected_groups(self):
        groups = []
        for checkbox in self.checkboxes:
            if checkbox.get_active():
                groups.append(checkbox.get_label())
        return groups

class Module(object):
    def __init__(self):
        try:
            self.builder = Gtk.Builder()
            self.builder.add_from_file("/usr/share/found-accounts-manager/window.ui")
            self.window = self.builder.get_object("main_window")
            self.window.connect("destroy", Gtk.main_quit)
            self.window.set_keep_above(True)
            self.window.set_title(_("Users and Groups"))
            self.builder.get_object("label_users").set_label(_("User Group"))
            self.builder.get_object("label_groups").set_label(_("User Groups Settings"))

            self.builder.get_object("button_add_group").connect("clicked", self.on_group_addition)
            self.builder.get_object("button_edit_group").connect("clicked", self.on_group_edition)
            self.builder.get_object("button_delete_group").connect("clicked", self.on_group_deletion)

            self.users = Gtk.TreeStore(object, GdkPixbuf.Pixbuf, str)
            self.users.set_sort_column_id(2, Gtk.SortType.ASCENDING)

            self.groups = Gtk.TreeStore(int, str)
            self.groups.set_sort_column_id(1, Gtk.SortType.ASCENDING)

            self.users_treeview = self.builder.get_object("treeview_users")
            self.users_treeview.set_rules_hint(True)

            self.groups_treeview = self.builder.get_object("treeview_groups")

            self.users_treeview.get_selection().connect("changed", self.on_user_selection)
            self.groups_treeview.get_selection().connect("changed", self.on_group_selection)

            column = Gtk.TreeViewColumn()
            cell = Gtk.CellRendererPixbuf()
            column.pack_start(cell, True)
            column.add_attribute(cell, 'pixbuf', INDEX_USER_PICTURE)
            cell.set_property('ypad', 1)
            self.users_treeview.append_column(column)

            column = Gtk.TreeViewColumn()
            cell = Gtk.CellRendererText()
            column.pack_start(cell, True)
            column.add_attribute(cell, 'markup', INDEX_USER_DESCRIPTION)
            self.users_treeview.append_column(column)

            column = Gtk.TreeViewColumn()
            cell = Gtk.CellRendererText()
            column.pack_start(cell, True)
            column.add_attribute(cell, 'text', INDEX_GROUPNAME)
            column.set_sort_column_id(1)
            self.groups_treeview.append_column(column)

            self.builder.get_object("button_edit_group").set_sensitive(False)
            self.builder.get_object("button_delete_group").set_sensitive(False)
            self.menu = Gtk.Menu()
            separator = Gtk.SeparatorMenuItem()

            row = 0
            col = 0
            num_cols = 4
            face_dirs = ["/usr/share/faces"]
            for face_dir in face_dirs:
                if os.path.exists(face_dir):
                    pictures = sorted(os.listdir(face_dir))
                    for picture in pictures:
                        path = os.path.join(face_dir, picture)
                        tmp_file = Gio.File.new_for_path(path)
                        file_icon = Gio.FileIcon.new(tmp_file)
                        image = Gtk.Image.new_from_gicon(file_icon, Gtk.IconSize.DIALOG)
                        menuitem = Gtk.MenuItem()
                        menuitem.add(image)
                        menuitem.connect('activate', self._on_face_menuitem_activated, path)
                        self.menu.attach(menuitem, col, col+1, row, row+1)
                        col = (col+1) % num_cols
                        if col == 0:
                            row = row + 1

            row = row + 1

            self.menu.attach(separator, 0, 4, row, row+1)

            self.account_type_combo = Gtk.ComboBoxText()
            self.account_type_combo.append_text(_("Standard"))
            self.account_type_combo.append_text(_("Administrator"))
            self.account_type_combo.connect("changed", self._on_accounttype_changed)

            self.groups_label = Gtk.Label(ellipsize=Pango.EllipsizeMode.END)
            self.groups_label.set_line_wrap(True)
            self.groups_label.set_alignment(0.5, 0.1)
            self.groups_label.set_lines(10)

            self.groups_button = Gtk.Button()
            self.groups_button.add(self.groups_label)
            self.groups_button.set_relief(Gtk.ReliefStyle.NONE)
            self.groups_button.set_tooltip_text(_("Click to change the groups. \nIf not work immediately after change groups, \nplease ensure operation correctly and restart system."))
            self.groups_button.connect("clicked", self._on_groups_button_clicked)

            box = Gtk.Box()

            table = DimmedTable()
            # table.add_labels([_("Picture"), _("Account Type"), _("Full Name"), _("Password"), _("Auto Login"), _("Groups")])
            # table.add_controls([box, self.account_type_combo, self.realname_entry, self.password_button, self.auto_login_box, self.groups_button])
            table.add_labels([ _("Groups")])
            table.add_controls([self.groups_button])

            self.builder.get_object("box_users").add(table)

            self.account_service = AccountsService.UserManager.get_default()
            self.account_service.connect('notify::is-loaded', self.on_accounts_service_loaded)

            self.load_groups()

            self.window.show_all()

            self.builder.get_object("box_users").hide()
            if len(sys.argv) > 1 and sys.argv[1] == '-g':
                #for i in sys.argv:
                 #   print i
                self.notebook = self.builder.get_object("notebook1")
                self.notebook.set_current_page(1)
                #print type(self.notebook)
                #print type(EditableEntry)#.set_current_page(1)
            #else: print "no sys argv"
        except Exception as detail:
            print(detail)



    def _on_groups_button_clicked(self, widget):
        model, treeiter = self.users_treeview.get_selection().get_selected()
        if treeiter != None:
            user = model[treeiter][INDEX_USER_OBJECT]
            dialog = GroupsDialog(user.get_user_name())
            response = dialog.run()
            if response == Gtk.ResponseType.OK:
                groups = dialog.get_selected_groups()
                os.system("usermod %s -G %s" % (user.get_user_name(), ",".join(groups)))
                groups.sort()
                self.groups_label.set_text(", ".join(groups))
            dialog.destroy()

    def _on_auto_login_clicked(self, switch, state):
        model, treeiter = self.users_treeview.get_selection().get_selected()
        if treeiter != None:
            user = model[treeiter][INDEX_USER_OBJECT]
            active = switch.get_active()
            if active != user.get_automatic_login():
                user.set_automatic_login(active)
                if user.get_automatic_login():
                    users = self.account_service.list_users()
                    for member in users:
                        if member != user:
                            user.set_automatic_login(False)
            # else:
            #     print "Set user %s automatic deactive." % user.get_user_name()

    #define len_too_long hint---jmx
    def tip(self, msg):
        dialog = Gtk.MessageDialog(None, Gtk.DialogFlags.DESTROY_WITH_PARENT, Gtk.MessageType.ERROR, Gtk.ButtonsType.OK, None)
        dialog.set_default_size(400, 75)
        dialog.set_markup(msg)
        dialog.show_all()
        response = dialog.run()
        dialog.destroy()
        return response == Gtk.ResponseType.OK

    def _on_accounttype_changed(self, combobox):
        model, treeiter = self.users_treeview.get_selection().get_selected()
        if treeiter != None:
            user = model[treeiter][INDEX_USER_OBJECT]
            if self.account_type_combo.get_active() == 1:
                user.set_account_type(AccountsService.UserAccountType.ADMINISTRATOR)
            else:
                user.set_account_type(AccountsService.UserAccountType.STANDARD)

            groups = []
            for group in grp.getgrall():
                if user.get_user_name() in group[3]:
                    groups.append(group[0])
            groups.sort()
            self.groups_label.set_text(", ".join(groups))



    def _on_face_browse_menuitem_activated(self, menuitem):
        model, treeiter = self.users_treeview.get_selection().get_selected()
        if treeiter != None:
            user = model[treeiter][INDEX_USER_OBJECT]
            dialog = Gtk.FileChooserDialog(_("Browse for more pictures..."), None, Gtk.FileChooserAction.OPEN, (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OPEN, Gtk.ResponseType.OK))
            file_chooser_filter = Gtk.FileFilter()
            file_chooser_filter.set_name(_("Images"))
            file_chooser_filter.add_mime_type("image/*")
            dialog.add_filter(file_chooser_filter)

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
                left = (width - new_width) / 2
                top = (height - new_height) / 2
                right = (width + new_width) / 2
                bottom = (height + new_height) / 2
                image = image.crop((left, top, right, bottom))
                image.thumbnail((96, 96), Image.LANCZOS)
                face_path = os.path.join(user.get_home_dir(), ".face")
                image.save(face_path, "png")
                user.set_icon_file(face_path)
                self.face_image.set_from_file(face_path)
                model.set_value(treeiter, INDEX_USER_PICTURE, GdkPixbuf.Pixbuf.new_from_file_at_size(face_path, 48, 48))
                model.row_changed(model.get_path(treeiter), treeiter)

            dialog.destroy()

    def update_preview_cb(self, dialog, preview):
        filename = dialog.get_preview_filename()
        dialog.set_preview_widget_active(False)
        if os.path.isfile(filename):
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(filename, 128, 128)
            if pixbuf is not None:
                preview.set_from_pixbuf(pixbuf)
                dialog.set_preview_widget_active(True)

    def _on_face_menuitem_activated(self, menuitem, path):
        if os.path.exists(path):
            model, treeiter = self.users_treeview.get_selection().get_selected()
            if treeiter != None:
                user = model[treeiter][INDEX_USER_OBJECT]
                user.set_icon_file(path)
                self.face_image.set_from_file(path)
                shutil.copy(path, os.path.join(user.get_home_dir(), ".face"))
                model.set_value(treeiter, INDEX_USER_PICTURE, GdkPixbuf.Pixbuf.new_from_file_at_size(path, 48, 48))
                model.row_changed(model.get_path(treeiter), treeiter)

    def menu_display(self, widget, event):
        if event.button == 1:
            self.menu.popup(None, None, self.popup_menu_below_button, self.face_button, event.button, event.time)
            self.menu.show_all()

    def popup_menu_below_button(self, menu, px, py, widget):
        # here I get the coordinates of the button relative to
        # window (self.window)
        button_x, button_y = widget.get_allocation().x, widget.get_allocation().y

        # now convert them to X11-relative
        unused_var, window_x, window_y = widget.get_window().get_origin()
        x = window_x + button_x
        y = window_y + button_y

        # now move the menu below the button
        y += widget.get_allocation().height

        push_in = True # push_in is True so all menu is always inside screen
        return (x, y, push_in)

    def on_accounts_service_loaded(self, user, param):
        self.load_users()

    def read_config_file_record(self):
        conf_settings = None
        config = configparser.ConfigParser()
        try:
            config.read(LIGHTDM_CONFIG_FILE)
            if config.has_option("SeatDefaults", "autologin-user"):
                conf_settings = config.get("SeatDefaults", "autologin-user")
        except Exception as e:
            print("read configuration file failed!")
        else:
            return conf_settings

    def load_users(self):
        #settings = self.read_config_file_record()
        self.users.clear()
        users = self.account_service.list_users()
        for user in users:
            if os.path.exists(user.get_icon_file()):
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(user.get_icon_file(), 48, 48)
            else:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size("/usr/share/faces/user-generic.png", 48, 48)
            description = "<b>%s</b>" % (user.get_real_name())
            piter = self.users.append(None, [user, pixbuf, description])
            #if (settings is not None) and (settings == user.get_user_name()):
            #    user.set_automatic_login(True)
        self.users_treeview.set_model(self.users)

    def load_groups(self):
        self.groups.clear()
        groups = sorted(grp.getgrall(), key=lambda x: x[0], reverse=False)
        for group in groups:
            (gr_name, gr_passwd, gr_gid, gr_mem) = group
            piter = self.groups.append(None, [gr_gid, gr_name])
        self.groups_treeview.set_model(self.groups)

    #USER CALLBACKS

    def on_user_selection(self, selection):
        model, treeiter = selection.get_selected()
        if treeiter != None:
            user = model[treeiter][INDEX_USER_OBJECT]

            groups = []
            for group in grp.getgrall():
                if user.get_user_name() in group[3]:
                    groups.append(group[0])
            groups.sort()
            self.groups_label.set_text(", ".join(groups))
            self.builder.get_object("box_users").show()
            connections = int(subprocess.getoutput("w -hs %s | wc -l" % user.get_user_name()))

    def judge_string_is_legal(self, string1):
        error_list = r'<>?,./:\";\'{}|[]\_+-=)(*&^%$#@!`~'
        for i, char in enumerate(string1):
            for j, echar in enumerate(error_list):
                if echar == char:
                    return False
        return True

    def judge_username_string_is_legal(self, string):
        if string[0].isdigit():
            return False
        for char in string:
            if char.isupper():
                return False
        return True

    def judge_username_string_is_legal(self, string):
        if string[0].isdigit():
            return False
        for char in string:
            if char.isupper():
                return False
        return True

    def userask(self, msg):
        dialog = Gtk.MessageDialog(None,
                                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                   Gtk.MessageType.ERROR,
                                   Gtk.ButtonsType.OK,
                                   None)
        dialog.set_default_size(400, 75)
        dialog.set_markup(msg)
        dialog.show_all()
        response = dialog.run()
        dialog.destroy()
        return response == Gtk.ResponseType.OK

   


    def on_user_edition(self, event):
        model, treeiter = self.users_treeview.get_selection().get_selected()
        if treeiter != None:
            print(("Editing user %s" % model[treeiter][INDEX_USER_OBJECT].get_user_name()))

    # GROUPS CALLBACKS

    def on_group_selection(self, selection):
        model, treeiter = selection.get_selected()
        if treeiter != None:
            self.builder.get_object("button_edit_group").set_sensitive(True)
            self.builder.get_object("button_edit_group").set_tooltip_text("")
            self.builder.get_object("button_delete_group").set_sensitive(True)
            self.builder.get_object("button_delete_group").set_tooltip_text("")
            group = model[treeiter][INDEX_GROUPNAME]
            for p in pwd.getpwall():
                username = p[0]
                primary_group = grp.getgrgid(p[3])[0]
                if primary_group == group and primary_group != "root":
                    self.builder.get_object("button_delete_group").set_sensitive(False)
                    self.builder.get_object("button_delete_group").set_tooltip_text(_("This group is set as %s's primary group") % username)
                    break

                if "root" == group or "sudo" == group:
                    self.builder.get_object("button_edit_group").set_sensitive(False)
                    self.builder.get_object("button_edit_group").set_tooltip_text(_("Selected group cannot be edit by %s") % username)
                    self.builder.get_object("button_delete_group").set_sensitive(False)
                    self.builder.get_object("button_delete_group").set_tooltip_text(_("Selected group cannot be delete by %s") % username)
                    break
        else:
            self.builder.get_object("button_edit_group").set_sensitive(False)
            self.builder.get_object("button_delete_group").set_sensitive(False)
            self.builder.get_object("button_delete_group").set_tooltip_text("")

    def on_group_deletion(self, event):
        model, treeiter = self.groups_treeview.get_selection().get_selected()
        if treeiter != None:
            group = model[treeiter][INDEX_GROUPNAME]
            message = _("Are you sure you want to permanently delete %s?") % group
            d = Gtk.MessageDialog(self.window,
                                  Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                  Gtk.MessageType.QUESTION,
                                  Gtk.ButtonsType.YES_NO,
                                  message)
            d.set_markup(message)
            d.set_default_response(Gtk.ResponseType.NO)
            r = d.run()
            if r == Gtk.ResponseType.YES:
                os.system("groupdel %s" % group)
                self.load_groups()
            d.destroy()

    def on_group_addition(self, event):
        dialog = GroupDialog(_("Group Name"), "")
        response = dialog.run()
        # modified by liting @ 2015-06-01
        namepat = re.compile(r'^[a-z_][a-z0-9_-]*[$]?')
        matcha = namepat.match(dialog.entry.get_text().lower())

        if response == Gtk.ResponseType.OK:
            if matcha.group() == dialog.entry.get_text():
                ret = os.system("groupadd %s" % matcha.group())
                if ret >> 8 != 0:
                    self.userask(_('The group is already exist!'))
                    dialog.destroy()
                    self.on_group_addition("clicked")
            # liting
                else:
                    self.load_groups()
            else:
                self.userask(_("Group name can not start with digits and can not contains invalid symbols."))
                dialog.destroy()
                self.on_group_addition("clicked")

        dialog.destroy()

    def on_group_edition(self, event):
        model, treeiter = self.groups_treeview.get_selection().get_selected()
        if treeiter != None:
            group = model[treeiter][INDEX_GROUPNAME]
            dialog = GroupDialog(_("Group Name"), group)
            response = dialog.run()
            # if response == Gtk.ResponseType.OK:
            #     os.system("groupmod %s -n %s" % (group, dialog.entry.get_text().lower()))
            #     self.load_groups()
            #     modified by liting @ 2015-06-10
            namepat = re.compile(r'^[a-z_][a-z0-9_-]*[$]?')
            matcha = namepat.match(dialog.entry.get_text().lower())

            if response == Gtk.ResponseType.OK:
                if matcha.group() == dialog.entry.get_text():
                    ret = os.system("groupmod %s -n %s" % (group, matcha.group()))
                    if ret >> 8 != 0:
                        self.userask(_('The group is already exist!'))
                        dialog.destroy()
                        self.on_group_edition("clicked")
                    else:
                        self.load_groups()
                else:
                    self.userask(_("Group name can not start with digits and can not contains invalid symbols."))
                    dialog.destroy()
                    self.on_group_edition("clicked")
            dialog.destroy()


if __name__ == "__main__":
    module = Module()
    Gtk.main()
