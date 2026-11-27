#define _GNU_SOURCE
#include <dlfcn.h>
#include <glib.h>
#include <locale.h>

static const struct {
    const gchar *original;
    const gchar *english;
    const gchar *vietnamese;
} lunar_labels[] = {
    { "情人节", "Valentine's Day", "Lễ Tình nhân" },
    { "妇女节", "Women's Day", "Ngày Phụ nữ" },
    { "植树节", "Arbor Day", "Ngày trồng cây" },
    { "劳动节", "Labour Day", "Ngày Quốc tế Lao động" },
    { "儿童节", "Children's Day", "Ngày Thiếu nhi" },
    { "建军节", "PLA Day", "Ngày thành lập Quân Giải phóng Nhân dân" },
    { "教师节", "Teachers' Day", "Ngày Nhà giáo" },
    { "国庆节", "China National Day", "Quốc khánh Trung Quốc" },
    { "元宵节", "Lantern Festival", "Tết Nguyên tiêu" },
    { "端午节", "Dragon Boat Festival", "Tết Đoan ngọ" },
    { "中元节", "Ghost Festival", "Lễ Trung nguyên" },
    { "中秋节", "Mid-Autumn Festival", "Tết Trung thu" },
    { "重阳节", "Double Ninth Festival", "Tết Trùng cửu" },
    { "腊八节", "Laba Festival", "Lễ Lạp Bát" },
    { "闰正月", "Leap month 1", "Tháng 1 nhuận" },
    { "闰二月", "Leap month 2", "Tháng 2 nhuận" },
    { "闰三月", "Leap month 3", "Tháng 3 nhuận" },
    { "闰四月", "Leap month 4", "Tháng 4 nhuận" },
    { "闰五月", "Leap month 5", "Tháng 5 nhuận" },
    { "闰六月", "Leap month 6", "Tháng 6 nhuận" },
    { "闰七月", "Leap month 7", "Tháng 7 nhuận" },
    { "闰八月", "Leap month 8", "Tháng 8 nhuận" },
    { "闰九月", "Leap month 9", "Tháng 9 nhuận" },
    { "闰十月", "Leap month 10", "Tháng 10 nhuận" },
    { "闰冬月", "Leap month 11", "Tháng 11 nhuận" },
    { "闰腊月", "Leap month 12", "Tháng 12 nhuận" },
    { "除夕", "Lunar New Year's Eve", "Giao thừa" },
    { "元旦", "New Year's Day", "Tết Dương lịch" },
    { "春节", "Lunar New Year", "Tết Nguyên đán" },
    { "七夕", "Qixi Festival", "Lễ Thất tịch" },
    { "下元", "Xiayuan Festival", "Lễ Hạ nguyên" },
    { "冬至", "Winter Solstice", "Đông chí" },
    { "小寒", "Minor Cold", "Tiểu hàn" },
    { "大寒", "Major Cold", "Đại hàn" },
    { "立春", "Start of Spring", "Lập xuân" },
    { "雨水", "Rain Water", "Vũ thủy" },
    { "惊蛰", "Awakening of Insects", "Kinh trập" },
    { "春分", "Spring Equinox", "Xuân phân" },
    { "清明", "Clear and Bright", "Thanh minh" },
    { "谷雨", "Grain Rain", "Cốc vũ" },
    { "立夏", "Start of Summer", "Lập hạ" },
    { "小满", "Grain Buds", "Tiểu mãn" },
    { "芒种", "Grain in Ear", "Mang chủng" },
    { "夏至", "Summer Solstice", "Hạ chí" },
    { "小暑", "Minor Heat", "Tiểu thử" },
    { "大暑", "Major Heat", "Đại thử" },
    { "立秋", "Start of Autumn", "Lập thu" },
    { "处暑", "End of Heat", "Xử thử" },
    { "白露", "White Dew", "Bạch lộ" },
    { "秋分", "Autumn Equinox", "Thu phân" },
    { "寒露", "Cold Dew", "Hàn lộ" },
    { "霜降", "Frost Descent", "Sương giáng" },
    { "立冬", "Start of Winter", "Lập đông" },
    { "小雪", "Minor Snow", "Tiểu tuyết" },
    { "大雪", "Major Snow", "Đại tuyết" },
    { "混沌", "Unknown", "Không rõ" },
    { "正月", "Month 1", "Tháng 1" },
    { "二月", "Month 2", "Tháng 2" },
    { "三月", "Month 3", "Tháng 3" },
    { "四月", "Month 4", "Tháng 4" },
    { "五月", "Month 5", "Tháng 5" },
    { "六月", "Month 6", "Tháng 6" },
    { "七月", "Month 7", "Tháng 7" },
    { "八月", "Month 8", "Tháng 8" },
    { "九月", "Month 9", "Tháng 9" },
    { "十月", "Month 10", "Tháng 10" },
    { "冬月", "Month 11", "Tháng 11" },
    { "腊月", "Month 12", "Tháng 12" },
    { "初一", "01", "01" },
    { "初二", "02", "02" },
    { "初三", "03", "03" },
    { "初四", "04", "04" },
    { "初五", "05", "05" },
    { "初六", "06", "06" },
    { "初七", "07", "07" },
    { "初八", "08", "08" },
    { "初九", "09", "09" },
    { "初十", "10", "10" },
    { "十一", "11", "11" },
    { "十二", "12", "12" },
    { "十三", "13", "13" },
    { "十四", "14", "14" },
    { "十五", "15", "15" },
    { "十六", "16", "16" },
    { "十七", "17", "17" },
    { "十八", "18", "18" },
    { "十九", "19", "19" },
    { "二十", "20", "20" },
    { "廿一", "21", "21" },
    { "廿二", "22", "22" },
    { "廿三", "23", "23" },
    { "廿四", "24", "24" },
    { "廿五", "25", "25" },
    { "廿六", "26", "26" },
    { "廿七", "27", "27" },
    { "廿八", "28", "28" },
    { "廿九", "29", "29" },
    { "三十", "30", "30" },
    { "甲子", "Jia-Zi", "Giáp Tý" },
    { "乙丑", "Yi-Chou", "Ất Sửu" },
    { "丙寅", "Bing-Yin", "Bính Dần" },
    { "丁卯", "Ding-Mao", "Đinh Mão" },
    { "戊辰", "Wu-Chen", "Mậu Thìn" },
    { "己巳", "Ji-Si", "Kỷ Tỵ" },
    { "庚午", "Geng-Wu", "Canh Ngọ" },
    { "辛未", "Xin-Wei", "Tân Mùi" },
    { "壬申", "Ren-Shen", "Nhâm Thân" },
    { "癸酉", "Gui-You", "Quý Dậu" },
    { "甲戌", "Jia-Xu", "Giáp Tuất" },
    { "乙亥", "Yi-Hai", "Ất Hợi" },
    { "丙子", "Bing-Zi", "Bính Tý" },
    { "丁丑", "Ding-Chou", "Đinh Sửu" },
    { "戊寅", "Wu-Yin", "Mậu Dần" },
    { "己卯", "Ji-Mao", "Kỷ Mão" },
    { "庚辰", "Geng-Chen", "Canh Thìn" },
    { "辛巳", "Xin-Si", "Tân Tỵ" },
    { "壬午", "Ren-Wu", "Nhâm Ngọ" },
    { "癸未", "Gui-Wei", "Quý Mùi" },
    { "甲申", "Jia-Shen", "Giáp Thân" },
    { "乙酉", "Yi-You", "Ất Dậu" },
    { "丙戌", "Bing-Xu", "Bính Tuất" },
    { "丁亥", "Ding-Hai", "Đinh Hợi" },
    { "戊子", "Wu-Zi", "Mậu Tý" },
    { "己丑", "Ji-Chou", "Kỷ Sửu" },
    { "庚寅", "Geng-Yin", "Canh Dần" },
    { "辛卯", "Xin-Mao", "Tân Mão" },
    { "壬辰", "Ren-Chen", "Nhâm Thìn" },
    { "癸巳", "Gui-Si", "Quý Tỵ" },
    { "甲午", "Jia-Wu", "Giáp Ngọ" },
    { "乙未", "Yi-Wei", "Ất Mùi" },
    { "丙申", "Bing-Shen", "Bính Thân" },
    { "丁酉", "Ding-You", "Đinh Dậu" },
    { "戊戌", "Wu-Xu", "Mậu Tuất" },
    { "己亥", "Ji-Hai", "Kỷ Hợi" },
    { "庚子", "Geng-Zi", "Canh Tý" },
    { "辛丑", "Xin-Chou", "Tân Sửu" },
    { "壬寅", "Ren-Yin", "Nhâm Dần" },
    { "癸卯", "Gui-Mao", "Quý Mão" },
    { "甲辰", "Jia-Chen", "Giáp Thìn" },
    { "乙巳", "Yi-Si", "Ất Tỵ" },
    { "丙午", "Bing-Wu", "Bính Ngọ" },
    { "丁未", "Ding-Wei", "Đinh Mùi" },
    { "戊申", "Wu-Shen", "Mậu Thân" },
    { "己酉", "Ji-You", "Kỷ Dậu" },
    { "庚戌", "Geng-Xu", "Canh Tuất" },
    { "辛亥", "Xin-Hai", "Tân Hợi" },
    { "壬子", "Ren-Zi", "Nhâm Tý" },
    { "癸丑", "Gui-Chou", "Quý Sửu" },
    { "甲寅", "Jia-Yin", "Giáp Dần" },
    { "乙卯", "Yi-Mao", "Ất Mão" },
    { "丙辰", "Bing-Chen", "Bính Thìn" },
    { "丁巳", "Ding-Si", "Đinh Tỵ" },
    { "戊午", "Wu-Wu", "Mậu Ngọ" },
    { "己未", "Ji-Wei", "Kỷ Mùi" },
    { "庚申", "Geng-Shen", "Canh Thân" },
    { "辛酉", "Xin-You", "Tân Dậu" },
    { "壬戌", "Ren-Xu", "Nhâm Tuất" },
    { "癸亥", "Gui-Hai", "Quý Hợi" },
    { "虚", "Unknown", "Không rõ" },
};

static gchar *
localize_lunar_label(const gchar *text)
{
    if (text == NULL || g_str_is_ascii(text) || !g_utf8_validate(text, -1, NULL))
        return NULL;

    const gchar *locale = setlocale(LC_TIME, NULL);
    const gboolean vietnamese = locale != NULL &&
        (g_str_equal(locale, "vi") || g_str_has_prefix(locale, "vi_") ||
         g_str_has_prefix(locale, "vi-"));
    GString *translated = g_string_new(NULL);

    // Only translate known calendar terms. Leave mixed or unrelated CJK labels alone.
    for (const gchar *part = text; *part != '\0';) {
        if (g_ascii_isspace(*part) || g_ascii_isdigit(*part) || *part == '/' || *part == '-') {
            g_string_append_c(translated, *part++);
            continue;
        }

        guint label;
        for (label = 0; label < G_N_ELEMENTS(lunar_labels); label++) {
            if (g_str_has_prefix(part, lunar_labels[label].original)) {
                g_string_append(translated, vietnamese ? lunar_labels[label].vietnamese :
                    lunar_labels[label].english);
                part += strlen(lunar_labels[label].original);
                break;
            }
        }
        if (label == G_N_ELEMENTS(lunar_labels)) {
            g_string_free(translated, TRUE);
            return NULL;
        }
    }
    return g_string_free(translated, FALSE);
}

gpointer
st_label_new(const gchar *text)
{
    gpointer (*new_label)(const gchar *) = dlsym(RTLD_NEXT, "st_label_new");
    g_autofree gchar *localized = localize_lunar_label(text);
    return new_label(localized != NULL ? localized : text);
}

void
st_label_set_text(gpointer label, const gchar *text)
{
    void (*set_text)(gpointer, const gchar *) = dlsym(RTLD_NEXT, "st_label_set_text");
    g_autofree gchar *localized = localize_lunar_label(text);
    set_text(label, localized != NULL ? localized : text);
}
