#include <cstdlib>
#include <dlfcn.h>
#include <libintl.h>
#include <locale.h>
#include <string_view>
#include <fcitx/action.h>
#include <fcitx/instance.h>
#include <fcitx/inputmethodentry.h>

namespace {
thread_local fcitx::Instance *activeInstance = nullptr;

struct ScopedInstance {
    explicit ScopedInstance(fcitx::Instance *instance)
        : previous(activeInstance) {
        activeInstance = instance;
    }
    ~ScopedInstance() { activeInstance = previous; }
    fcitx::Instance *previous;
};

const fcitx::InputMethodEntry *vietnameseInputMethod(fcitx::InputContext *context) {
    if (!activeInstance) {
        return nullptr;
    }
    if (!context) {
        context = activeInstance->mostRecentInputContext();
    }
    const auto *entry = context ? activeInstance->inputMethodEntry(context) : nullptr;
    if (!entry) {
        return nullptr;
    }
    const auto &code = entry->languageCode();
    auto language = std::string_view(code);
    language = language.substr(0, language.find_first_of("_-.@"));
    return language == "vi" || language == "vie" ? entry : nullptr;
}

std::string translateAction(const std::string &text, const fcitx::InputMethodEntry &entry) {
    static const locale_t vietnamese = newlocale(
        LC_MESSAGES_MASK | LC_CTYPE_MASK, "vi_VN.UTF-8", nullptr);
    if (!vietnamese) {
        return text;
    }

    const std::string addonDomain = "fcitx5-" + entry.addon();
    const locale_t previous = uselocale(vietnamese);
    if (!previous) {
        return text;
    }

    const char *translation = dgettext("fcitx5", text.c_str());
    if (translation == text && !entry.addon().empty()) {
        translation = dgettext(addonDomain.c_str(), text.c_str());
    }
    uselocale(previous);
    return translation;
}
}

// Fcitx caches the Classic UI action labels when it creates the tray.
// Resolve their language when the menu reads them for an input context.
int fcitx::Instance::exec() {
    using Exec = int (*)(Instance *);
    static const auto original = reinterpret_cast<Exec>(
        dlsym(RTLD_NEXT, "_ZN5fcitx8Instance4execEv"));
    if (!original) {
        return EXIT_FAILURE;
    }
    const ScopedInstance scope(this);
    return original(this);
}

std::string fcitx::SimpleAction::shortText(InputContext *context) const {
    using ShortText = std::string (*)(const SimpleAction *, InputContext *);
    static const auto original = reinterpret_cast<ShortText>(
        dlsym(RTLD_NEXT, "_ZNK5fcitx12SimpleAction9shortTextB5cxx11EPNS_12InputContextE"));
    if (!original) {
        return {};
    }

    const auto text = original(this, context);
    if (!text.empty()) {
        if (const auto *entry = vietnameseInputMethod(context)) {
            return translateAction(text, *entry);
        }
    }
    return text;
}
