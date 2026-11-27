#!/usr/bin/env python3
# -*- coding: utf-8 -*-


import os
import os.path
import subprocess
import gi
import re
import sys
import gettext
import operator
import tempfile
import importlib
import lxml.etree
import _thread as thread
import foundtools.config as ftconfig
gi.require_version('Gtk', '3.0')
gi.require_version('GdkPixbuf', '2.0')
gi.require_version('Pango', '1.0')
from gi.repository import Pango, Gio, GLib, GdkPixbuf, Gtk
from PIL import Image
importlib.reload(sys)
from concurrent.futures import ThreadPoolExecutor

gettext.bindtextdomain(ftconfig.GETTEXT_PACKAGE, ftconfig.localedir)
gettext.textdomain(ftconfig.GETTEXT_PACKAGE)
_ = gettext.gettext

LOCK_DELAY_OPTIONS = [
    (15, _("15 seconds")),
    (30, _("30 seconds")),
    (60, _("1 minute")),
    (300, _("5 minutes")),
    (600, _("10 minutes")),
    (1800, _("30 minutes")),
    (3600, _("1 hour")),
    (21600, _("6 hours")),
    (86400, _("1 day")),
]

TIMEOUT_LOCK_DELAY_OPTIONS = [
    (300, _("5 minutes")),
    (600, _("10 minutes")),
    (900, _("15 minutes")),
]

SUPPORTED_FILE_TYPE = [
    'png', 'jpg', 'bmp', 'gif', 'jpeg', 'svg', 'tif',
    'tiff', 'tga', 'fpx', 'cdr', 'dxf', 'exif',
    'ufo', 'eps', 'raw', 'win', 'xbm', 'pic', 'pnt', 'psd', 'ai',
    'ico', 'pcx', 'pict', 'pict2', 'lbm', 'opt', 'msk', 'ppm'
]

def get_field_value():
    try:
        with open("/etc/vos/info", 'r', encoding='utf-8') as file:
            for line in file:
                line = line.strip()
                if line.startswith("CUSTOM_INFO" + "="):
                    return line.split("=", 1)[1].strip().strip('"')
        return None
    except Exception as e:
        print(f"Error: {e}")
        return None

SCREENSAVER_ICON_VIEW_ITEM_WIDTH = 106
SCREENSAVER_ICON_VIEW_ITEM_HEIGHT = 60
SCREENSAVER_PREVIEW_WIDTH = 160
SCREENSAVER_PREVIEW_HEIGHT = 90
BACKGROUND_PROPERTIES = "/usr/share/background-properties"
LOGIN_BACKGROUND_DEFAULT = "/usr/share/vos-login-qt/sources/accounts"
LOGIN_BACKGROUND_KEY = "use-for-greeter"
if get_field_value() == "sugon" or get_field_value() == "hygoncpu":
    BACKGROUNDS_FOLDER_PATH = "/usr/share/backgrounds/hygoncpu/"
    BACKGROUND_DEFAULT = "/usr/share/backgrounds/hygoncpu/HyGon_default.svg"
else:
    BACKGROUNDS_FOLDER_PATH = "/usr/share/backgrounds/found"
    BACKGROUND_DEFAULT = "/usr/share/backgrounds/found/default.svg"


def file_set_new_name(old_name):
    new_name = re.sub(r'&+(?!(lt;|gt;|quot;|apos;|amp;))', "&amp;", old_name)
    new_name = re.sub(r'<', "&lt;", new_name)
    new_name = re.sub(r'>', "&gt;", new_name)
    new_name = re.sub(r'"', "&quot;", new_name)
    new_name = re.sub(r'\'', "&apos;", new_name)
    new_name = re.sub(r'#', "", new_name)
    new_name = re.sub(r'%', "", new_name)
    print(("Screensaver: File set new name: {}".format(new_name)))
    return new_name


def file_name_parse(file_name):
    matched = re.search(r'<|>|"|\'|#|%|&+(?!(gt;|quot;|apos;|amp;))', file_name)
    new_name = file_name
    if matched and matched.group():
        new_name = file_set_new_name(file_name)
    return new_name

def should_show_greeter_button():
    if not os.path.exists(LOGIN_BACKGROUND_DEFAULT):
        return False
    if not os.access(LOGIN_BACKGROUND_DEFAULT, os.W_OK):
        return False
    return True

class FindFirstImageException(Exception):
    def __init__(self):
        super(FindFirstImageException, self).__init__()
        self.message = "Screensaver: Find first image failed"


class PixCache(object):
    """ Create picture cache """
    def __init__(self):
        self._data = {}

    def scale_pix_buf(self, filename, width, height):
        tmp_pix_buf = GdkPixbuf.Pixbuf.new_from_file(filename)
        if tmp_pix_buf.get_width() != SCREENSAVER_ICON_VIEW_ITEM_WIDTH or tmp_pix_buf.get_height() != SCREENSAVER_ICON_VIEW_ITEM_HEIGHT:
            tmp_pix_buf = tmp_pix_buf.scale_simple(SCREENSAVER_ICON_VIEW_ITEM_WIDTH,
                                                SCREENSAVER_ICON_VIEW_ITEM_HEIGHT,
                                                GdkPixbuf.InterpType.BILINEAR)
        pix = [tmp_pix_buf, width, height]
        return pix

    def get_pix(self, filename, size=None):
        try:
            mimetype = subprocess.check_output(["file", "-bi", filename]).decode().split(";")[0]
            if not mimetype.startswith("image/"):
                print(("Screensaver: Not try to covert %s, not a recognized image file" % filename))
                return None
        except Exception as detail:
            print(("Screensaver: Exception, failed to detect mimetype for {}, because {}".format(filename, detail)))
            return None

        pix = None

        if not filename in self._data:
            self._data[filename] = {}

        if size in self._data[filename]:
            # if origin size same as thumbnail size, don't create thumbnail
            pix = self._data[filename][size]
        else:
            try:
                if mimetype == "image/svg+xml":
                    pix = self.scale_pix_buf(filename, SCREENSAVER_ICON_VIEW_ITEM_WIDTH, SCREENSAVER_ICON_VIEW_ITEM_HEIGHT)
                else:
                    img = Image.open(filename)
                    (width, height) = img.size
                    if img.mode != 'RGB':
                        img = img.convert('RGB')
                    if size:
                        img.thumbnail((size, size), Image.LANCZOS)

                    # Convert Image -> Pixbuf(save to file, GTK3 is not reliable for that)
                    f = tempfile.NamedTemporaryFile(delete=False)
                    temp_filename = f.name
                    f.close()
                    img.save(temp_filename, "png")
                    pix = self.scale_pix_buf(temp_filename, width, height)
                    os.unlink(temp_filename)
            except Exception as detail:
                print(("Screensaver: Exception, failed to convert {}, because {}".format(filename, detail)))
                pix = None

            if pix:
                self._data[filename][size] = pix
        return pix


PIX_CACHE = PixCache()


class ScreensaverIconView(Gtk.IconView):
    def __init__(self):
        super(ScreensaverIconView, self).__init__()
        self.set_item_width(SCREENSAVER_ICON_VIEW_ITEM_WIDTH)
        self._model = Gtk.ListStore(object, GdkPixbuf.Pixbuf, str)
        self.set_model(self._model)
        self.set_name("ScreensaverIconView")
        self.set_columns(4)

        area = self.get_area()
        self.set_row_spacing(1)
        self.set_column_spacing(1)
        self.set_selection_mode(Gtk.SelectionMode.SINGLE)

        # set pixbuf renderer
        pixbuf_renderer = Gtk.CellRendererPixbuf()
        pixbuf_renderer.set_fixed_size(SCREENSAVER_ICON_VIEW_ITEM_WIDTH, SCREENSAVER_ICON_VIEW_ITEM_HEIGHT)

        # set text renderer, set ellipsize mode to end
        text_renderer = Gtk.CellRendererText(ellipsize=Pango.EllipsizeMode.END)
        text_renderer.set_alignment(0.5, 0)
        text_renderer.set_property("alignment", Pango.Alignment.CENTER)

        area.pack_start(pixbuf_renderer, False, False, 0)
        area.pack_start(text_renderer, False, False, 0)
        self.add_attribute(pixbuf_renderer, "pixbuf", 1)
        self.add_attribute(text_renderer, "markup", 2)

        self._loading_queue = []
        # create a new lock object
        self._loading_queue_lock = thread.allocate_lock()

        self._loading = False
        # create a new lock object
        self._loading_lock = thread.allocate_lock()

        self._loaded_data = []
        # create a new lock object
        self._loaded_data_lock = thread.allocate_lock()

    def set_pictures_list(self, pictures_list):
        self.clear()
        for i in pictures_list:
            self.add_picture(i)

    def clear(self):
        # get lock
        self._loading_queue_lock.acquire()
        self._loading_queue = []
        # release lock
        self._loading_queue_lock.release()

        self._loading_lock.acquire()
        is_loading = self._loading
        self._loading_lock.release()
        while is_loading:
            time.sleep(0.1)
            self._loading_lock.acquire()
            is_loading = self._loading
            self._loading_lock.release()

        self._model.clear()

    def add_picture(self, picture):
        self._loading_queue_lock.acquire()
        self._loading_queue.append(picture)
        self._loading_queue_lock.release()

        start_loading = False
        self._loading_lock.acquire()
        if not self._loading:
            self._loading = True
            start_loading = True
        self._loading_lock.release()

        if start_loading:
            GLib.timeout_add(100, self._check_loading_progress)
            thread.start_new_thread(self._do_load, ())

    def _check_loading_progress(self):
        self._loading_lock.acquire()
        self._loaded_data_lock.acquire()
        res = self._loading
        to_load = []
        loaded_data_length = 0
        if self._loaded_data:
            loaded_data_length = len(self._loaded_data)
        while loaded_data_length > 0:
            to_load.append(self._loaded_data[0])
            self._loaded_data = self._loaded_data[1:]
            loaded_data_length = len(self._loaded_data)
        self._loading_lock.release()
        self._loaded_data_lock.release()

        for i in to_load:
            self._model.append(i)

        return res

    def _do_load(self):
        finished = False
        def process_load_item(to_load):
            filename = to_load["filename"]
            if filename.endswith(".xml"):
                filename = self.get_first_file_from_screensaver_xml(filename)
            pix = PIX_CACHE.get_pix(filename, SCREENSAVER_ICON_VIEW_ITEM_WIDTH)
            if pix != None:
                if "name" in to_load:
                    label = to_load["name"]
                else:
                    label = os.path.split(to_load["filename"])[1]
                self._loaded_data_lock.acquire()
                self._loaded_data.append((to_load, pix[0], "<b>%s</b>" % label))
                self._loaded_data_lock.release()

        with ThreadPoolExecutor(max_workers=2) as executor:
            while not finished:
                self._loading_queue_lock.acquire()
                loading_queue_length = 0
                if self._loading_queue:
                    loading_queue_length = len(self._loading_queue)
                if loading_queue_length == 0:
                    finished = True
                else:
                    to_load = self._loading_queue[0]
                    self._loading_queue = self._loading_queue[1:]
                self._loading_queue_lock.release()
                if not finished:
                    executor.submit(process_load_item, to_load)
        self._loading_lock.acquire()
        self._loading = False
        self._loading_lock.release()

    def get_first_file_from_screensaver_xml(self, filename):
        try:
            f = open(filename)
            root_node = lxml.etree.fromstring(f.read())
            f.close()
            if root_node.tag == "background":
                for background_node in root_node:
                    if background_node.tag == "static":
                        for static_node in backgroundnode:
                            if static_node.tag == "file":
                                return static_node.text
            print(("Screensaver: Could not find filename in %s" % filename))
            return None
        except Exception as detail:
            print(("Screensaver: Failed to read filename from %s: %s" % (filename, detail)))
            return None


class ScreensaverWallpaperPane(Gtk.Box):
    def __init__(self, main_box, schema):
        super(ScreensaverWallpaperPane, self).__init__(orientation=Gtk.Orientation.VERTICAL)
        self.schema = schema
        self.main_box = main_box
        self.custom_screensaver_path = os.path.join(os.getenv("HOME"), ".cdos", "screensaver")

        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scw.set_shadow_type(Gtk.ShadowType.ETCHED_IN)
        self.pack_start(scw, True, True, 0)
        self.icon_view = ScreensaverIconView()
        scw.add(self.icon_view)
        self.icon_view.connect("selection-changed", self._on_select_changed)
        # self.icon_view.connect("item-activated", self._on_selection_changed)
        self.update_icon_view()
        self.main_box.apply_login_button.connect("clicked", self._on_apply_login_button_clicked)

    def get_selected_wallpaper(self):
        selected_items = self.icon_view.get_selected_items()
        if len(selected_items) == 1:
            path = selected_items[0]
            tree_iter = self.icon_view.get_model().get_iter(path)
            return self.icon_view.get_model().get(tree_iter, 0)[0]
        return None

    def remove_item(self):
        # single selection
        selected_item = self.get_selected_wallpaper()
        # print("Screensaver: Remove_item in wallpaper: {}".format(selected_item["filename"]))
        if not selected_item:
            return
        self.schema.set_string("picture-uri", BACKGROUND_DEFAULT)
        os.unlink(selected_item["filename"])
        self.main_box.update_preview(False, BACKGROUND_DEFAULT)
        self.update_icon_view()

    def remove_login_background(self):
        user_name = GLib.get_user_name()
        Files = os.listdir(LOGIN_BACKGROUND_DEFAULT)
        for k in range(len(Files)):
            if os.path.splitext(Files[k])[0] == user_name :
                os.system("rm -f {}/{}".format(LOGIN_BACKGROUND_DEFAULT, Files[k]))
                print("Screensaver: delete  login path files : {}/{}".format(LOGIN_BACKGROUND_DEFAULT, Files[k]))

    def _on_apply_login_button_clicked(self, widget):
        apply_login_state = self.schema.get_boolean(LOGIN_BACKGROUND_KEY)
        current_picture_uri = self.schema.get_string("picture-uri")
        if not apply_login_state:
            self.remove_login_background()
        else:
            user_name = GLib.get_user_name()
            image_type = current_picture_uri[-4:]
            tmp_login_uri = "{}/{}{}".format(LOGIN_BACKGROUND_DEFAULT, user_name, image_type)
            os.system("cp -f {} {}".format(current_picture_uri, tmp_login_uri))
            print("Screensaver: copy image to login_path: {}".format(current_picture_uri))

    def _on_select_changed(self, iconview):
        tree_path = iconview.get_selected_items()
        if len(tree_path) == 1:
            model = iconview.get_model()
            tree_iter = model.get_iter(tree_path)
            # wallpaper_node = model.get(tree_iter, 0)
            wallpaper_pixbuf = model.get(tree_iter, 1)[0]
            item_pixbuf = wallpaper_pixbuf.scale_simple(SCREENSAVER_PREVIEW_WIDTH, SCREENSAVER_PREVIEW_HEIGHT,
                                                        GdkPixbuf.InterpType.BILINEAR)
            wallpaper_name = model.get(tree_iter, 2)[0]
            self.main_box.preview_image.set_from_pixbuf(item_pixbuf)
            self.main_box.preview_name.set_markup(wallpaper_name)
            file_path = model.get(tree_iter, 0)[0]["filename"]
            if file_path != self.schema.get_string("picture-uri"):
                self.schema.set_string("picture-uri", file_path)

            # Forbid delete cdos-backgrounds pictures, even user is root
            if os.access(file_path, os.W_OK) and file_path.startswith(self.custom_screensaver_path):
                self.main_box.remove_button.set_sensitive(True)
            else:
                self.main_box.remove_button.set_sensitive(False)
            apply_login_state = self.schema.get_boolean(LOGIN_BACKGROUND_KEY)
            user_name = GLib.get_user_name()
            if should_show_greeter_button():
                if apply_login_state:
                    image_type = file_path[-4:]
                    tmp_login_uri = "{}/{}{}".format(LOGIN_BACKGROUND_DEFAULT, user_name, image_type)
                    os.system("cp -f {} {}".format(file_path, tmp_login_uri))
                    print("Screensaver: copy image to login_path: {}".format(tmp_login_uri))
                else:
                    self.remove_login_background()
        else:
            self.main_box.remove_button.set_sensitive(False)
        return

    # def _on_selection_changed(self, iconview, path):
    #     # TODO
    #     pass

    def split_locale_code(self, locale_code):
        print(("Screensaver: Default is {}, local_code is {}".format(locale.getdefaultlocale(), locale_code)))
        loc = locale_code.partition("_")
        loc = (loc[0], loc[2])
        return loc

    def get_local_wallpaper_name(self, names, loc):
        result = ""
        main_loc_found = False
        for wp in names:
            wp_loc = wp[0]
            wp_name = wp[1]
            if wp_loc == ("", ""):
                if not main_loc_found:
                    result = wp_name
            elif wp_loc[0] == loc[0]:
                if wp_loc[1] == loc[1]:
                    return wp_name
                elif wp_loc[1] == "":
                    result = wp_name
                    main_loc_found = True
        return result

    def parse_xml_screensaver_list(self, filename):
        """get file list from parse xml file

        This is written in a way that compatible with xml:lang="LOCAL_NAME", but no used.
        Localized name is used as the content of the "name" filed, using wp_name directly
        whitout using split_locale_code and get_local_wallpaper_name function

        Args:
            filename: xml for store picture metadata.

        Returns:
            A file name list or None.
        """
        try:
            # print('func:parse_xml_screensaver_list, filename:{}'.format(filename))
            # loc_attr_name = "{http://www.w3.org/XML/1998/namespace}lang"
            # loc = self.split_locale_code(locale.getdefaultlocale()[0])
            res = []
            # sub_locale_found = False
            tmp_file = open(filename)
            root_node = lxml.etree.fromstring(tmp_file.read())
            tmp_file.close()
            # get root node, parse xml file
            if root_node.tag == "wallpapers":
                for wallpaper_node in root_node:
                    if wallpaper_node.tag == "wallpaper" and wallpaper_node.get("deleted") != "true":
                        wallpaper_data = {"metadataFile": filename}
                        names = []
                        for prop in wallpaper_node:
                            if isinstance(prop.tag, str):
                                if prop.tag != "name":
                                    wallpaper_data[prop.tag] = prop.text
                                else:
                                    # prop_attr = prop.attrib
                                    wp_name = prop.text
                                    # if prop_attr.has_key(loc_attr_name):
                                    #     loc_name = self.split_locale_code(prop_attr.get(loc_attr_name))
                                    # else:
                                    #     loc_name = ("", "")
                                    loc_name = ("", "")
                                    names.append((loc_name, wp_name))
                        # wallpaper_data["name"] = self.get_local_wallpaper_name(names, loc)
                        wallpaper_data["name"] = wp_name

                        # filename in wallpaper_data, not None, filename's path exists, and have write permission
                        if "filename" in wallpaper_data and \
                            wallpaper_data["filename"] != "" and \
                            os.path.exists(wallpaper_data["filename"]) and \
                            os.access(wallpaper_data["filename"], os.R_OK):
                            if wallpaper_data["name"] == "":
                                wallpaper_data["name"] = os.path.basename(wallpaper_data["filename"])
                            res.append(wallpaper_data)
            return res
        except Exception as e:
            print(("Screensaver: Occur exception, {}".format(e)))
            return []

    def update_icon_view(self):
        pictures_list = []
        # if os.path.exists(BACKGROUND_DEFAULT):
        #     DEFAULT_NODE = {'name':'wallpaper', 'artist': 'CDOS', 'filename': '{}'.format(BACKGROUND_DEFAULT)}
            # pictures_list.append(DEFAULT_NODE)

        xml_list = []
        if os.path.exists(BACKGROUND_PROPERTIES):
            for file_name in os.listdir(BACKGROUND_PROPERTIES):
                file_path = os.path.join(BACKGROUND_PROPERTIES, file_name)
                if not os.path.isdir(file_path) and file_name.endswith(".xml"):
                    xml_list.append(file_path)

        default_xml_file = os.path.join(BACKGROUND_PROPERTIES, "default","found.xml")
        if os.path.exists(default_xml_file):
            pictures_list += self.parse_xml_screensaver_list(default_xml_file)
        for xml_file in xml_list:
            pictures_list += self.parse_xml_screensaver_list(xml_file)

        path = os.path.join(os.getenv("HOME"), ".cdos", "screensaver")
        if os.path.exists(path):
            # add from custom folder
            for i in os.listdir(path):
                filename = os.path.join(path, i)
                new_filename = file_name_parse(filename)
                if operator.eq(new_filename, filename):
                    os.rename(filename, new_filename)
                    filename = new_filename
                cmd = "file -bi \"%s\"" % re.sub(r'\$', "\\$", filename)
                if subprocess.check_output(cmd, shell=True).decode().split(";")[0].startswith("image/"):
                    pictures_list.append({"name": os.path.splitext(os.path.basename(filename))[0], "filename": filename})
        self.icon_view.set_pictures_list(pictures_list)


class AddWallpapersDialog(Gtk.FileChooserDialog):
    def __init__(self, parent=None):
        Gtk.FileChooserDialog.__init__(self, title=_("Add wallpapers"), transient_for=parent)
        self.add_button(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL)
        self.add_button(Gtk.STOCK_OPEN, Gtk.ResponseType.OK)

        preview = Gtk.Image()
        self.set_preview_widget(preview)
        # Customization GtkFileChooserDialog
        try:
            place_view = self.get_children()[0].get_children()[0].get_children()[0].get_children()[0].get_children()[1]
            place_view_location_bar = place_view.get_children()[0]
            place_view_location_bar.hide()
            place_sidebar = self.get_children()[0].get_children()[0].get_children()[0].get_children()[0].get_children()[0]
            place_sidebar.set_show_other_locations(False)
            place_sidebar.set_show_recent(False)
            place_sidebar.set_show_trash(False)
            place_sidebar.set_local_only(False)
            place_sidebar.set_show_enter_location(False)
            #place_sidebar.set_show_connect_to_server(False)
            self.set_current_folder(os.path.expanduser("~"))
            list_box = place_sidebar.get_children()[0].get_children()[0]
            list_box.set_filter_func(self.list_box_filter_func, None)
        except Exception as e:
            print("Screensaver: Customization FileChooserDialog error, may be Gtk update")
        # Customization GtkFileChooserDialog end
        self.connect("update-preview", self.update_icon_preview_cb, preview)

    def list_box_filter_func(self, row, user_data):
        label = row.get_children()[0].get_children()[0].get_children()[0].get_children()[1]
        t = label.get_text()
        for item in [_("Home"), _("Desktop"), _("Documents"), _("Downloads"), _("Music"), _("Pictures"), _("Video"), "Desktop"]:
            if t == item:
                return True
        return False

    def run(self):
        self.show_all()
        resp = Gtk.FileChooserDialog.run(self)
        self.hide()
        if resp == Gtk.ResponseType.OK:
            res = self.get_filenames()
        else:
            res = []
        return res

    def update_icon_preview_cb(self, chooser, preview):
        filename = chooser.get_preview_filename()
        if filename is None:
            return
        chooser.set_preview_widget_active(False)
        if os.path.isfile(filename):
            mimetype = subprocess.check_output(["file", "-bi", filename]).decode().split(";")[0]
            if mimetype.startswith("image/gif") or mimetype.startswith("image/tiff"):
                return
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(filename, 128, 128)
            if pixbuf is not None:
                preview.set_from_pixbuf(pixbuf)
                chooser.set_preview_widget_active(True)


class PathListBoxItem(Gtk.ListBoxRow):
    def __init__(self, folder_name, folder_path):
        super(PathListBoxItem, self).__init__()
        self.hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        self.set_size_request(-1, 66)
        self.add(self.hbox)

        self.header_img = Gtk.Image.new_from_file("/usr/share/found-screensaver-manager/screensaver-folder.svg")
        self.header_img.set_margin_start(12)
        header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        header_box.pack_start(self.header_img, False, False, 0)
        self.hbox.pack_start(header_box, False, False, 0)

        self.name_label = Gtk.Label()
        self.name_label.set_label(folder_name)
        name_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        name_box.set_halign(Gtk.Align.START)
        name_box.set_name("name-box")
        name_box.pack_start(self.name_label, True, True, 0)
        self.hbox.pack_start(name_box, True, True, 1)

        self.checked_img = Gtk.Image()
        self.checked_img.set_margin_end(12)
        selected_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        selected_box.pack_start(self.checked_img, False, False, 0)
        self.hbox.pack_start(selected_box, False, False, 2)
        self.folder_path = folder_path
        self.folder_name = folder_name

    def set_checked_status(self, is_selected):
        if is_selected:
            self.checked_img.set_from_file("/usr/share/found-screensaver-manager/screensaver-check.svg")
        else:
            self.checked_img.clear()


class PathListBox(Gtk.ListBox):
    def __init__(self, main_box, schema):
        super(PathListBox, self).__init__()
        self.set_selection_mode(Gtk.SelectionMode.SINGLE)
        # self.xml_folder = os.path.join(os.getenv("HOME"), ".cdos", "screensaver")
        self.xml_folder = os.path.expanduser("~/.cdos/screensaver")
        self.xml_file = os.path.join(self.xml_folder, "slideshow.xml")
        self.path_list = []
        self.main_box = main_box
        self.schema = schema
        self.create_xml()
        self.update_row_node_from_xml()

    def path_already_exists(self, folder_path):
        if folder_path in self.path_list:
            return True
        return False

    def add_row(self, folder_name, folder_path):
        if self.add_row_node_to_xml(folder_name, folder_path):
            self.schema.set_string("picture-uri", folder_path)
            self.update_row_node_from_xml()

    def remove_row(self, folder_name, folder_path):
        if self.remove_row_node_from_xml(folder_name, folder_path):
            self.schema.set_string("picture-uri", BACKGROUNDS_FOLDER_PATH)
            self.update_row_node_from_xml()

    def update_row_node_from_xml(self):
        try:
            xml_parser = lxml.etree.XMLParser(remove_blank_text=True)
            tree = lxml.etree.parse(self.xml_file, parser=xml_parser)
            root_node = tree.getroot()
            index = 0
            has_custom_define = False
            self.path_list = []
            if root_node.tag == "screensavers":
                for i in self:
                    self.remove(i)
                default_row = PathListBoxItem(_("DesktopBackgrounds"), BACKGROUNDS_FOLDER_PATH)
                self.path_list.append(BACKGROUNDS_FOLDER_PATH)
                self.add(default_row)
                node_num = len(root_node)
                while node_num != 0:
                    screensaver_node = root_node[index]
                    # no necessary to transform to encoding utf-8, because read from utf-8 xml
                    new_row = PathListBoxItem(screensaver_node[0].text, screensaver_node[1].text)
                    if self.schema.get_string("picture-uri") == new_row.folder_path:
                        if self.main_box.try_find_first_image(new_row.folder_path):
                            self.select_row(new_row)
                            new_row.set_checked_status(True)
                            has_custom_define = True
                            self.path_list.append(new_row.folder_path)
                            self.add(new_row)
                        else:
                            print("Screensaver: Custom pictures folder path has no picture, delete and reset")
                            self.remove_row_node_from_xml(new_row.folder_name, new_row.folder_path)
                    else:
                        self.path_list.append(new_row.folder_path)
                        self.add(new_row)
                    index = index + 1
                    if index == len(root_node):
                        break
                # cannot find custom define
                if not has_custom_define and self.check_not_picture():
                    self.select_row(default_row)
                    default_row.set_checked_status(True)
                    self.main_box.remove_button.set_sensitive(False)
                    self.schema.set_string("picture-uri", default_row.folder_path)
                self.show_all()
                return
            print(("Screensaver: Could not find root node in {} when update row".format(self.xml_file)))
            return
        except Exception as detail:
            print(("Screensaver: Failed to update from {} when update row, because {}".format(self.xml_file, detail)))
            return

    def check_not_picture(self):
        current_picture_uri = self.schema.get_string("picture-uri")
        if os.path.exists(current_picture_uri) and not os.path.isdir(current_picture_uri):
            return False
        return True

    def add_row_node_to_xml(self, folder_name, folder_path):
        try:
            # if have no permission, will raise OSError exception
            tmp_list = os.listdir(folder_path)
            # permission check end

            xml_parser = lxml.etree.XMLParser(remove_blank_text=True)
            tree = lxml.etree.parse(self.xml_file, parser=xml_parser)
            root_node = tree.getroot()
            if root_node.tag == "screensavers":
                new_row = lxml.etree.SubElement(root_node, 'screensaver')
                name_child = lxml.etree.SubElement(new_row, 'name')
                # must be unicode or ascii
                name_child.text = '{}'.format(folder_name)
                # must be unicode or ascii
                path_child = lxml.etree.SubElement(new_row, 'path')
                path_child.text = '{}'.format(folder_path)
                row_iter = new_row
                tree.write(self.xml_file, pretty_print=True, xml_declaration=True, encoding='utf-8')
                return True
            print(("Screensaver: Could not find root node in {} when add row".format(self.xml_file)))
            return False
        except Exception as detail:
            print(("Screensaver: Failed to parser from {} when add row, because {}".format(self.xml_file, detail)))
            return False

    def remove_row_node_from_xml(self, folder_name, folder_path):
        try:
            xml_parser = lxml.etree.XMLParser(remove_blank_text=True)
            tree = lxml.etree.parse(self.xml_file, parser=xml_parser)
            root_node = tree.getroot()
            if root_node.tag == "screensavers":
                for i in root_node.findall("screensaver"):
                    if i.find("path").text == folder_path:
                        root_node.remove(i)
                tree.write(self.xml_file, pretty_print=True, xml_declaration=True, encoding='utf-8')
                return True
            print(("Screensaver: Remove row node faild in {}, lost root node".format(self.xml_file)))
            return False
        except Exception as detail:
            print(("Screensaver: Remvoe row node occur exception, because {}".format(detail)))
            return False

    def create_xml(self):
        # index from xml
        if os.path.exists(self.xml_file):
            return
        if not os.path.exists(self.xml_folder):
            os.makedirs(self.xml_folder, 0o755)
        root_node = lxml.etree.Element("screensavers")
        tree = lxml.etree.ElementTree(root_node)
        res = tree.write(self.xml_file, pretty_print=True, xml_declaration=True, encoding='utf-8')


class ScreensaverSlideshowPane(Gtk.Box):
    def __init__(self, main_box, schema):
        super(ScreensaverSlideshowPane, self).__init__(orientation=Gtk.Orientation.VERTICAL)
        self.schema = schema
        self.main_box = main_box

        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scw.set_shadow_type(Gtk.ShadowType.ETCHED_IN)
        self.pack_start(scw, True, True, 0)
        self.list_box = PathListBox(main_box, schema)
        self.list_box.connect("row-selected", self._on_row_selected)
        self.last_activated_row = None
        scw.add(self.list_box)

    def reset_default(self):
        default_row = self.list_box.get_row_at_index(0)
        self.list_box.select_row(default_row)

    def _on_row_selected(self, list_box, selected_row):
        if selected_row and selected_row.folder_path == self.schema.get_string("picture-uri"):
            return

        index = 0
        item_row = list_box.get_row_at_index(index)
        # remove all items checked status image
        while item_row:
            item_row.set_checked_status(False)
            index = index + 1
            item_row = list_box.get_row_at_index(index)

        if selected_row:
            selected_row.set_checked_status(True)
            self.schema.set_string("picture-uri", selected_row.folder_path)
            self.main_box.update_preview(True, selected_row.folder_path)
            if selected_row.folder_name == _("DesktopBackgrounds"):
                self.main_box.remove_button.set_sensitive(False)
                return

        self.main_box.remove_button.set_sensitive(True)

    def add_item(self, folder_path):
        folder_name = os.path.basename(folder_path)
        try:
            # check has permission
            items = os.listdir(folder_path)
        except Exception as e:
            print(("Screensaver: Add item failed, because {}".format(e)))
            return
        if not self.list_box.path_already_exists(folder_path):
            if self.main_box.update_preview(True, folder_path):
                self.list_box.add_row(folder_name, folder_path)
            else:
                print("Screensaver: You selected forlder path has no files")

    def remove_item(self):
        selected_row = self.list_box.get_selected_row()
        if not selected_row or selected_row.get_index() == 0:
            return
        self.list_box.remove_row(selected_row.folder_name, selected_row.folder_path)

    def get_selected_slideshow(self):
        selected_row = self.get_selected_row()
        return selected_row


class Module:
    def __init__(self):
        # keywords = _("screensaver, lock, password, away, message")
        # sidePage = SidePage(_("Screen Locker"), "cs-screensaver1", keywords, content_box, module=self)
        # self.sidePage = sidePage
        # self.name = "screensaver"
        # self.category = "prefs"
        # self.comment = _("Manage screen lock settings")
        #self.screensaver_checkbutton = None
        self.type_tree_view = None
        self.first_iter = None
        self.second_iter = None
        self.add_button = None
        self.remove_button = None
        self.apply_login_button = None
        self.interval_combo_button = None
        self.interval_button_box = None
        self.interval_combo_button_added = False
        self.select_wallpaper_box = None
        self.on_module_selected()

    def on_type_selection_changed(self, selection):
        model, tree_view_iter = selection.get_selected()
        if not tree_view_iter:
            print("Screensaver: None type be select")
            return

        # print("Screensaver: You select is %s" % model[tree_view_iter][0])
        for i in self.select_wallpaper_box:
            self.select_wallpaper_box.remove(i)
        if model[tree_view_iter][0] == _("Pictures"):
            self.select_wallpaper_box.add(self.screensaver_pane)
            self.current_type = "pictures"
            self.interval_combo_button.hide()
            if should_show_greeter_button():
                self.apply_login_button.show()
            current_image_path = self.cdos_screensaver_schema.get_string("picture-uri")
            self.update_preview(False, current_image_path)
        else:
            self.select_wallpaper_box.add(self.slideshow_pane)
            self.current_type = "slideshow"
            if not self.interval_combo_button_added:
                self.interval_button_box.pack_start(self.interval_combo_button, False, False, 0)
                self.interval_combo_button_added = True
            self.interval_combo_button.show()
            self.apply_login_button.hide()
            current_image_path = self.cdos_slideshow_schema.get_string("picture-uri")
            self.update_preview(True, current_image_path)

        self.select_wallpaper_box.show_all()
        if self.remove_button:
            self.update_remove_button(self.remove_button)

    def create_type_tree_view(self):
        # create list store
        type_list_store = Gtk.ListStore(str)
        type_list_store.append([_("Pictures")])
        type_list_store.append([_("Slide Show")])

        # set tree view
        self.type_tree_view = Gtk.TreeView(model=type_list_store)
        self.type_tree_view.set_headers_visible(False)
        self.type_tree_view.set_margin_top(10)

        # create column
        column = Gtk.TreeViewColumn("Type")
        self.type_tree_view.append_column(column)

        # set column
        cell_renderer_text = Gtk.CellRendererText()
        cell_renderer_text.set_fixed_size(-1, 35)
        column.pack_start(cell_renderer_text, True)
        column.add_attribute(cell_renderer_text, "text", 0)

        return self.type_tree_view

    def update_remove_button(self, btn):
        btn_status = True
        if self.current_type == "pictures":
            selected_item = self.screensaver_pane.get_selected_wallpaper()
            if not selected_item:
                btn_status = False
            elif not os.access(selected_item["filename"], os.W_OK):
                btn_status = False
        elif self.current_type == "slideshow":
            selected_row = self.slideshow_pane.list_box.get_selected_row()
            if not selected_row or selected_row.get_index() == 0:
                btn_status = False
        btn.set_sensitive(btn_status)

    def _on_remove_button_clicked(self, btn):
        if self.current_type == "pictures":
            self.screensaver_pane.remove_item()
        elif self.current_type == "slideshow":
            self.slideshow_pane.remove_item()

    def _on_add_button_clicked(self, btn):
        self._add_screensavers_error_dialog = Gtk.MessageDialog(parent = None,
                                                                # flags = Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                                                destroy_with_parent = True,
                                                                message_type = Gtk.MessageType.ERROR,
                                                                buttons = Gtk.ButtonsType.OK,
                                                                text = None)
        self._add_screensavers_error_dialog.set_default_size(200, 50)
        self._add_screensavers_error_dialog.set_name("screensavers-add-error-dialog")
        self._add_screensavers_dialog = AddWallpapersDialog()
        # distinguish between selecting files or folders
        if self.current_type == "pictures":
            self._add_screensavers_dialog.set_action(Gtk.FileChooserAction.OPEN)
            self._add_screensavers_dialog.set_select_multiple(True)
            pic_filter = Gtk.FileFilter()
            pic_filter.add_pixbuf_formats()
            self._add_screensavers_dialog.set_filter(pic_filter)
        else:
            self._add_screensavers_dialog.set_action(Gtk.FileChooserAction.SELECT_FOLDER)
        filenames = self._add_screensavers_dialog.run()
        if self.current_type == "pictures":
            if filenames:
                # dest_dir = os.path.join(os.getenv("HOME"), ".cdos", "screensaver")
                dest_dir = os.path.expanduser("~/.cdos/screensaver")
                if not os.path.exists(dest_dir):
                    rec_mkdir(dest_dir)
                for filename in filenames:
                    dest_filename = os.path.join(dest_dir, os.path.split(filename)[1])
                    if os.path.split(filename)[1] != '' and os.path.split(filename)[1].find(".") > 0:
                        dest_filename_fix = os.path.split(filename)[1].split('.')[-1].lower()
                        if dest_filename_fix in SUPPORTED_FILE_TYPE:
                            if not os.path.exists(filename):
                                msg = _("This file does not exist.")
                                self._add_screensavers_error_dialog.set_markup(msg)
                                error = self._add_screensavers_error_dialog.run()
                                self._add_screensavers_error_dialog.destroy()
                                return
                            dest_filename = file_set_new_name(dest_filename)
                            fs = open(filename, 'rb')
                            fd = open(dest_filename, 'wb')
                            fd.write(fs.read())

                            fs.close()
                            fd.close()
                        else:
                            msg = _("You should choose a picture.")
                            self._add_screensavers_error_dialog.set_markup(msg)
                            error = self._add_screensavers_error_dialog.run()
                            self._add_screensavers_error_dialog.destroy()
                            return
                    else:
                        msg = _("This path does not exist.")
                        self._add_screensavers_error_dialog.set_markup(msg)
                        error = self._add_screensavers_error_dialog.run()
                        self._add_screensavers_error_dialog.destroy()
                        return

                self.screensaver_pane.update_icon_view()
        else:
            for folder_path in filenames:
                self.slideshow_pane.add_item(folder_path)

    def update_preview(self, is_slideshow, path):
        print("preview path: {}".format(path))
        if path and GLib.str_has_prefix (path, "file://"):
            path = GLib.filename_from_uri (path)[0]
            print("new path: {}".format(path))

        if is_slideshow:
            try:
                image_path = self.try_find_first_image(path)
                if image_path is None:
                    raise FindFirstImageException()
            except FindFirstImageException as e:
                print(("{}, reset default".format(e.message)))
                image_path = None
                return False
            except Exception as e:
                print(("Screensaver: Find first image failed, because {}".format(e)))
                image_path = None
                return False
            self.preview_name.set_text(_("Slide Show"))
        else:
            image_path = path
            self.preview_name.set_markup("<b>{}</b>".format(os.path.splitext(os.path.basename(image_path))[0]))
        print("image_path: {}".format(image_path))
        pixbuf_tmp = GdkPixbuf.Pixbuf.new_from_file(image_path)
        pixbuf_tmp = pixbuf_tmp.scale_simple(SCREENSAVER_PREVIEW_WIDTH,
                                             SCREENSAVER_PREVIEW_HEIGHT,
                                             GdkPixbuf.InterpType.BILINEAR)
        self.preview_image.set_from_pixbuf(pixbuf_tmp)
        return True

    def try_find_first_image(self, slideshow_path):
        items = os.listdir(slideshow_path)
        for item in items:
            # hide file or folder
            if item[0] == '.':
                continue
            item_full_path = os.path.join(slideshow_path, item)
            if not os.path.isdir(item_full_path):
                if os.path.splitext(item)[1][1:] in SUPPORTED_FILE_TYPE:
                    return item_full_path
        return None

    # def on_screensaver_picture_changed(self, settings, key):
    #     print("Test=>current settings: {}".format(settings.get_string("picture-uri")))

    def on_module_selected(self):
        try:
            self.cdos_screensaver_schema = Gio.Settings(schema="org.cdos.desktop.background.screensaver")
            self.settings = Gio.Settings(schema="org.cdos")
            self.cdos_screensaver_schema.set_boolean("lock-enabled", True)
            self.cdos_slideshow_schema = Gio.Settings(schema="org.cdos.desktop.background.slideshow")
            # self.cdos_screensaver_schema.connect("changed::picture-uri", self.on_screensaver_picture_changed)
            self.vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
            self.vbox.set_margin_start(35)
            self.vbox.set_margin_end(35)
            self.vbox.set_margin_top(35)
            self.vbox.set_margin_bottom(35)

            # add preview box
            preview_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            preview_box.set_margin_bottom(10)
            preview_image_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            preview_image_box.set_size_request(210, 90)
            self.preview_image = Gtk.Image()
            preview_image_box.pack_start(self.preview_image, True, True, 0)
            preview_image_box.set_name("screensaver-preview-box")
            preview_box.pack_start(preview_image_box, False, False, 0)
            preview_info_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
            preview_info_box.set_size_request(498, -1)
            # empty box, place holder
            preview_empty_box1 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            preview_info_box.pack_start(preview_empty_box1, True, True, 0)
            preview_label_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            preview_label = Gtk.Label()
            preview_label.set_label(_("Current Settings"))
            preview_label_box.pack_start(preview_label, False, False, 0)
            preview_info_box.pack_start(preview_label_box, False, False, 1)
            preview_name_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            self.preview_name = Gtk.Label()
            self.preview_name.set_text('')
            self.preview_name.set_margin_top(8)
            preview_name_box.pack_start(self.preview_name, False, False, 0)
            preview_info_box.pack_start(preview_name_box, False, False, 2)
            # empty box, place holder
            preview_empty_box2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            preview_info_box.pack_start(preview_empty_box2, True, True, 3)
            preview_info_box.set_name("screensaver-info-box")

            screensaver_settings_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            user_name = GLib.get_user_name()
            if user_name == 'secadm':
                is_secadm = True
            else:
                is_secadm = False

            timeout_combo_button = GSettingsSpecialVersionComboBox(_("Screensaver Timeout:"),
                                                                   "org.cdos.desktop.session",
                                                                   "idle-delay",
                                                                   is_secadm,
                                                                   TIMEOUT_LOCK_DELAY_OPTIONS,
                                                                   use_uint=True)

            timeout_combo_button.set_name("screensaver-timeout-button")
            timeout_combo_button.set_tooltip_text(_("This is session idle-delay, save this value to opt file which will be used by all other users."))
            screensaver_settings_box.pack_start(timeout_combo_button, False, False, 3)
            screensaver_settings_box.set_halign(Gtk.Align.END)
            #preview_name_box.pack_end(screensaver_settings_box, False, False, 0)
            preview_box.pack_start(preview_info_box, False, False, 1)
            self.vbox.pack_start(preview_box, False, False, 0)

            # add wallpaper box
            wallpaper_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            wallpaper_box.set_margin_bottom(6)
            type_tree_view_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            type_tree_view_box.set_size_request(210, 375)
            type_tree_view_box.show()
            type_tree_view_box.set_name("screensaver-type-box")
            # handle GtkTreeView
            self.create_type_tree_view()
            selection = self.type_tree_view.get_selection()
            # set Pictures as default select item start
            model, tree_view_iter = selection.get_selected()
            self.first_iter = model.get_iter_first()
            '''
            if self.first_iter:
                current_picture_uri = self.cdos_screensaver_schema.get_string("picture-uri")
                if not os.path.exists(current_picture_uri):
                    selection.select_iter(self.first_iter)
                    self.cdos_screensaver_schema.set_string("picture-uri", BACKGROUND_DEFAULT)
                    self.current_type = "pictures"
                elif os.path.isdir(current_picture_uri):
                    self.second_iter = model.iter_next(self.first_iter)
                    selection.select_iter(self.second_iter)
                    self.current_type = "slideshow"
                else:
                    selection.select_iter(self.first_iter)
                    self.current_type = "pictures"
            else:
                print("Screensaver: Cannot get any type tree view")
            # set Pictures as default select item end
            '''
            # Default Use current_type = "pictures"
            self.current_type = "pictures"
            selection.connect("changed", self.on_type_selection_changed)
            type_tree_view_box.pack_start(self.type_tree_view, True, True, 0)
            wallpaper_box.pack_start(type_tree_view_box, False, False, 0)
            add_wallpaper_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
            add_wallpaper_box.set_size_request(498, 380)
            add_wallpaper_box.set_name("screensaver-add-box")
            self.select_wallpaper_box = Gtk.EventBox()
            self.select_wallpaper_box.set_visible_window(False)
            add_wallpaper_box.pack_start(self.select_wallpaper_box, True, True, 0)
            self.select_wallpaper_box.set_name("screensaver-select-box")
            # add buttons
            buttons_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
            buttons_box.set_name("screensaver-buttons-box")
            add_button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            add_button_box.set_margin_start(12)
            add_button_box.set_margin_top(16)
            add_button_box.set_margin_bottom(16)
            self.add_button = Gtk.Button.new_with_label(_("Add from computer"))
            self.add_button.connect("clicked", self._on_add_button_clicked)
            # self.add_button.set_no_show_all(True)
            add_button_box.pack_start(self.add_button, False, False, 0)
            self.add_button.set_name("screensaver-add-button")
            buttons_box.pack_start(add_button_box, False, False, 0)
            remove_button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            remove_button_box.set_margin_top(16)
            remove_button_box.set_margin_bottom(16)
            self.remove_button = Gtk.Button.new_with_label(_("Delete"))
            self.remove_button.set_name("screensaver-remove-button")
            self.remove_button.connect("clicked", self._on_remove_button_clicked)
            remove_button_box.pack_start(self.remove_button, False, False, 0)
            buttons_box.pack_start(remove_button_box, False, False, 1)

            self.apply_login_button = GSettingsCheckButton(_("Use for login greeter background"), "org.cdos.desktop.background.screensaver", LOGIN_BACKGROUND_KEY, None)
            self.apply_login_button.set_tooltip_text(_("The selected picture will be used for login greeter background"))
            buttons_box.pack_end(self.apply_login_button, False, False, 3)
            self.apply_login_button.set_no_show_all(True)
            self.apply_login_button.hide()

            # only for placeholder, no visible widget
            empty_button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            # empty_button_box.set_name("screensaver-empty-button")
            empty_button_box.set_margin_top(16)
            empty_button_box.set_margin_bottom(16)
            buttons_box.pack_start(empty_button_box, True, True, 2)
            self.interval_button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            self.interval_button_box.set_margin_top(16)
            self.interval_button_box.set_margin_bottom(16)
            self.interval_button_box.set_margin_end(8)
            self.interval_combo_button = GSettingsIntComboBox(_("Switching Frequency:"),
                                                              "org.cdos.desktop.background.slideshow",
                                                              "delay",
                                                              "org.cdos.desktop.background.slideshow/slideshow-enabled",
                                                              LOCK_DELAY_OPTIONS,
                                                              use_uint=False)
            self.interval_combo_button.set_name("screensaver-interval-button")
            self.interval_combo_button.set_tooltip_text(_("This option define pictures switching frequency in slideshow screensaver"))
            buttons_box.pack_start(self.interval_button_box, False, False, 3)
            add_wallpaper_box.pack_start(buttons_box, False, False, 1)
            wallpaper_box.pack_start(add_wallpaper_box, True, True, 1)
            self.vbox.pack_start(wallpaper_box, False, False, 1)
            # add main widget
            self.screensaver_pane = ScreensaverWallpaperPane(self, self.cdos_screensaver_schema)
            self.slideshow_pane = ScreensaverSlideshowPane(self, self.cdos_slideshow_schema)
            current_image_path = self.cdos_screensaver_schema.get_string("picture-uri")
            if self.current_type == "pictures":
                self.select_wallpaper_box.add(self.screensaver_pane)
                if os.path.exists(current_image_path) and os.path.isdir(current_image_path):
                    self.cdos_screensaver_schema.set_string("picture-uri", BACKGROUND_DEFAULT)
                    current_image_path = BACKGROUND_DEFAULT
                self.update_preview(False, current_image_path)
                if should_show_greeter_button():
                    self.apply_login_button.show()
            else:
                self.select_wallpaper_box.add(self.slideshow_pane)
                self.interval_button_box.pack_start(self.interval_combo_button, False, False, 0)
                self.interval_combo_button_added = True
                if os.path.exists(current_image_path) and os.path.isdir(current_image_path):
                    self.update_preview(True, current_image_path)
            self.update_remove_button(self.remove_button)

            # add checkbutton box
            # removed to found-control-center

            # is_special = self.settings.get_boolean("special-version")
            # if not is_special:
            #     checkbutton_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            #     checkbutton_box.set_margin_bottom(20)
            #     checkbutton_box.set_name("screensaver-checkbutton-box")
            #     self.screensaver_checkbutton = GSettingsCheckButton(_("Lock the computer when put to sleep"),
            #                                                         "org.cdos.settings-daemon.plugins.power",
            #                                                         "lock-on-suspend", None)
            #     self.screensaver_checkbutton.set_tooltip_text(_("Enable this option to require a password when the computer wakes up from suspend"))
            #     checkbutton_box.pack_start(self.screensaver_checkbutton, False, False, 0)
            #     self.vbox.pack_start(checkbutton_box, False, False, 2)

            add_wallpaper_box.set_name("add-wallpaper-box")

            self.dialog = Gtk.Dialog()
            self.dialog.set_title(_("Custom Settings"))
            self.dialog.connect("destroy", Gtk.main_quit)
            self.dialog.vbox.add(self.vbox)
            self.dialog.show_all()

        except Exception as detail:
            print(detail)

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
        self.content_widget.set_wrap_width(1)

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

            timeoutstr="timeout: %d:%d:%d" %(0, value/60, 0)
            with open('/opt/cdos-screensaver/cdos-screensaver.config', 'w+') as the_file:
                the_file.write(timeoutstr)


if __name__ == "__main__":
    module = Module()
    Gtk.main()
