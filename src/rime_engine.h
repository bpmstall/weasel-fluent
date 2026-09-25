#ifndef RIME_ENGINE_H
#define RIME_ENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QLibrary>
#include <cstdint>

constexpr int RIME_KEY_BACKSPACE = 0xff08;
constexpr int RIME_KEY_TAB = 0xff09;
constexpr int RIME_KEY_RETURN = 0xff0d;
constexpr int RIME_KEY_ESCAPE = 0xff1b;
constexpr int RIME_KEY_SPACE = 0x0020;
constexpr int RIME_KEY_LEFT = 0xff51;
constexpr int RIME_KEY_UP = 0xff52;
constexpr int RIME_KEY_RIGHT = 0xff53;
constexpr int RIME_KEY_DOWN = 0xff54;
constexpr int RIME_KEY_PAGE_UP = 0xff55;
constexpr int RIME_KEY_PAGE_DOWN = 0xff56;

struct CandidateItem {
    int index = 0;
    QString text;
    QString comment;
};

struct RimeUiState {
    bool isComposing = false;
    QString preedit;
    int cursorPos = 0;
    QVector<CandidateItem> candidates;
    int highlightedIndex = 0;
    int pageNo = 0;
    bool isLastPage = false;
    QString commitText;
};

class RimeEngine : public QObject {
    Q_OBJECT

public:
    explicit RimeEngine(QObject* parent = nullptr);
    ~RimeEngine() override;

    bool initialize(const QString& dllPath = QString(),
                    const QString& sharedDir = QString(),
                    const QString& userDir = QString());

    bool isLoaded() const { return m_isLoaded; }

    bool processKey(int keyCode, int mask = 0);
    bool processChar(QChar ch);
    bool selectCandidate(int index);

    RimeUiState getUiState();
    QString getCommitText();
    void clear();
    void commitDirectText(const QString& text);

    bool deploy();

    bool isAsciiMode() const { return m_asciiMode; }
    void setAsciiMode(bool ascii);

    bool isFullShape() const { return m_fullShape; }
    void setFullShape(bool full);

    bool isSimplified() const { return m_simplified; }
    void setSimplified(bool simp);

    QString currentSchema() const { return m_currentSchema; }
    bool selectSchema(const QString& schemaId);

signals:
    void stateChanged(const RimeUiState& state);
    void committed(const QString& text);

private:
    bool initRimeApi();
    void updateState();

    // Rime C API structures
    struct RimeTraits {
        int data_size;
        const char* shared_data_dir;
        const char* user_data_dir;
        const char* distribution_name;
        const char* distribution_code_name;
        const char* distribution_version;
        const char* app_name;
        const char** modules;
        int min_log_level;
        const char* log_dir;
        const char* prebuilt_data_dir;
        const char* staging_dir;
    };

    struct RimeComposition {
        int length;
        int cursor_pos;
        int sel_start;
        int sel_end;
        const char* preedit;
    };

    struct RimeCandidate {
        const char* text;
        const char* comment;
        void* reserved;
    };

    struct RimeMenu {
        int page_size;
        int page_no;
        int is_last_page;
        int highlighted_candidate_index;
        int num_candidates;
        RimeCandidate* candidates;
        const char* select_keys;
    };

    struct RimeContext {
        int data_size;
        RimeComposition composition;
        RimeMenu menu;
        const char* commit_text_preview;
        const char** select_labels;
    };

    struct RimeCommit {
        int data_size;
        const char* text;
    };

    struct RimeCandidateListIterator {
        void* ptr;
        int index;
        RimeCandidate candidate;
    };

    using FnRimeSetup = void (*)(RimeTraits*);
    using FnRimeInitialize = void (*)(RimeTraits*);
    using FnRimeStartMaintenance = int (*)(int);
    using FnRimeJoinMaintenanceThread = void (*)();
    using FnRimeCreateSession = uintptr_t (*)();
    using FnRimeDestroySession = int (*)(uintptr_t);
    using FnRimeProcessKey = int (*)(uintptr_t, int, int);
    using FnRimeGetContext = int (*)(uintptr_t, RimeContext*);
    using FnRimeFreeContext = int (*)(RimeContext*);
    using FnRimeGetCommit = int (*)(uintptr_t, RimeCommit*);
    using FnRimeFreeCommit = int (*)(RimeCommit*);
    using FnRimeSelectCandidate = int (*)(uintptr_t, size_t);
    using FnRimeCandidateListBegin = int (*)(uintptr_t, RimeCandidateListIterator*);
    using FnRimeCandidateListNext = int (*)(RimeCandidateListIterator*);
    using FnRimeCandidateListEnd = void (*)(RimeCandidateListIterator*);
    using FnRimeDeployWorkspace = int (*)();
    using FnRimeGetOption = int (*)(uintptr_t, const char*);
    using FnRimeSetOption = void (*)(uintptr_t, const char*, int);
    using FnRimeSelectSchema = int (*)(uintptr_t, const char*);

    QLibrary m_lib;
    bool m_isLoaded = false;
    uintptr_t m_sessionId = 0;

    FnRimeSetup m_fnSetup = nullptr;
    FnRimeInitialize m_fnInitialize = nullptr;
    FnRimeStartMaintenance m_fnStartMaintenance = nullptr;
    FnRimeJoinMaintenanceThread m_fnJoinMaintenanceThread = nullptr;
    FnRimeCreateSession m_fnCreateSession = nullptr;
    FnRimeDestroySession m_fnDestroySession = nullptr;
    FnRimeProcessKey m_fnProcessKey = nullptr;
    FnRimeGetContext m_fnGetContext = nullptr;
    FnRimeFreeContext m_fnFreeContext = nullptr;
    FnRimeGetCommit m_fnGetCommit = nullptr;
    FnRimeFreeCommit m_fnFreeCommit = nullptr;
    FnRimeSelectCandidate m_fnSelectCandidate = nullptr;
    FnRimeCandidateListBegin m_fnCandidateListBegin = nullptr;
    FnRimeCandidateListNext m_fnCandidateListNext = nullptr;
    FnRimeCandidateListEnd m_fnCandidateListEnd = nullptr;
    FnRimeDeployWorkspace m_fnDeployWorkspace = nullptr;
    FnRimeGetOption m_fnGetOption = nullptr;
    FnRimeSetOption m_fnSetOption = nullptr;
    FnRimeSelectSchema m_fnSelectSchema = nullptr;

    QByteArray m_sharedDirBytes;
    QByteArray m_userDirBytes;
    QByteArray m_appNameBytes;

    // Simulation / Standalone state
    bool m_asciiMode = false;
    bool m_fullShape = false;
    bool m_simplified = true;
    QString m_currentSchema = QStringLiteral("rime_ice");
    QString m_simInput;
    int m_simHighlight = 0;
};

#endif // RIME_ENGINE_H
