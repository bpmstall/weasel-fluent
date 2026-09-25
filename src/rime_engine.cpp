#include "rime_engine.h"

#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>
#include <cstring>

RimeEngine::RimeEngine(QObject* parent) : QObject(parent) {
}

RimeEngine::~RimeEngine() {
    if (m_isLoaded && m_sessionId && m_fnDestroySession) {
        m_fnDestroySession(m_sessionId);
        m_sessionId = 0;
    }
}

bool RimeEngine::initialize(const QString& dllPath,
                            const QString& sharedDir,
                            const QString& userDir) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString actualDll = dllPath;
    if (actualDll.isEmpty()) {
        if (QFileInfo::exists(appDir + QStringLiteral("/rime.dll"))) {
            actualDll = appDir + QStringLiteral("/rime.dll");
        } else {
            actualDll = QStringLiteral("C:/Program Files/Rime/weasel-0.17.4/rime.dll");
        }
    }

    if (QFileInfo::exists(actualDll)) {
        m_lib.setFileName(actualDll);
        if (m_lib.load() && initRimeApi()) {
            QString actualShared = sharedDir;
            if (actualShared.isEmpty()) {
                if (QFileInfo::exists(appDir + QStringLiteral("/data"))) {
                    actualShared = appDir + QStringLiteral("/data");
                } else {
                    actualShared = QStringLiteral("C:/Program Files/Rime/weasel-0.17.4/data");
                }
            }

            QString actualUser = userDir;
            if (actualUser.isEmpty()) {
                if (QFileInfo::exists(appDir + QStringLiteral("/user_dicts"))) {
                    actualUser = appDir + QStringLiteral("/user_dicts");
                } else {
                    actualUser = QStringLiteral("C:/Users/zheng/AppData/Roaming/Rime");
                }
            }

            m_sharedDirBytes = actualShared.toUtf8();
            m_userDirBytes = actualUser.toUtf8();
            m_appNameBytes = "rime.weasel-fluent-cpp-qt";

            RimeTraits traits{};
            traits.data_size = static_cast<int>(sizeof(RimeTraits) - sizeof(int));
            traits.shared_data_dir = m_sharedDirBytes.constData();
            traits.user_data_dir = m_userDirBytes.constData();
            traits.app_name = m_appNameBytes.constData();

            m_fnSetup(&traits);
            m_fnInitialize(&traits);

            if (m_fnStartMaintenance && m_fnStartMaintenance(0) != 0) {
                if (m_fnJoinMaintenanceThread) {
                    m_fnJoinMaintenanceThread();
                }
            }

            m_sessionId = m_fnCreateSession();
            if (m_sessionId != 0) {
                m_isLoaded = true;
                return true;
            }
        }
    }

    // Fallback to simulation mode
    m_isLoaded = false;
    return true;
}

bool RimeEngine::initRimeApi() {
    m_fnSetup = reinterpret_cast<FnRimeSetup>(m_lib.resolve("RimeSetup"));
    m_fnInitialize = reinterpret_cast<FnRimeInitialize>(m_lib.resolve("RimeInitialize"));
    m_fnStartMaintenance = reinterpret_cast<FnRimeStartMaintenance>(m_lib.resolve("RimeStartMaintenance"));
    m_fnJoinMaintenanceThread = reinterpret_cast<FnRimeJoinMaintenanceThread>(m_lib.resolve("RimeJoinMaintenanceThread"));
    m_fnCreateSession = reinterpret_cast<FnRimeCreateSession>(m_lib.resolve("RimeCreateSession"));
    m_fnDestroySession = reinterpret_cast<FnRimeDestroySession>(m_lib.resolve("RimeDestroySession"));
    m_fnProcessKey = reinterpret_cast<FnRimeProcessKey>(m_lib.resolve("RimeProcessKey"));
    m_fnGetContext = reinterpret_cast<FnRimeGetContext>(m_lib.resolve("RimeGetContext"));
    m_fnFreeContext = reinterpret_cast<FnRimeFreeContext>(m_lib.resolve("RimeFreeContext"));
    m_fnGetCommit = reinterpret_cast<FnRimeGetCommit>(m_lib.resolve("RimeGetCommit"));
    m_fnFreeCommit = reinterpret_cast<FnRimeFreeCommit>(m_lib.resolve("RimeFreeCommit"));
    m_fnSelectCandidate = reinterpret_cast<FnRimeSelectCandidate>(m_lib.resolve("RimeSelectCandidate"));
    if (!m_fnSelectCandidate) {
        m_fnSelectCandidate = reinterpret_cast<FnRimeSelectCandidate>(m_lib.resolve("RimeSelectCandidateOnCurrentPage"));
    }
    m_fnCandidateListBegin = reinterpret_cast<FnRimeCandidateListBegin>(m_lib.resolve("RimeCandidateListBegin"));
    m_fnCandidateListNext = reinterpret_cast<FnRimeCandidateListNext>(m_lib.resolve("RimeCandidateListNext"));
    m_fnCandidateListEnd = reinterpret_cast<FnRimeCandidateListEnd>(m_lib.resolve("RimeCandidateListEnd"));
    m_fnDeployWorkspace = reinterpret_cast<FnRimeDeployWorkspace>(m_lib.resolve("RimeDeployWorkspace"));
    m_fnGetOption = reinterpret_cast<FnRimeGetOption>(m_lib.resolve("RimeGetOption"));
    m_fnSetOption = reinterpret_cast<FnRimeSetOption>(m_lib.resolve("RimeSetOption"));
    m_fnSelectSchema = reinterpret_cast<FnRimeSelectSchema>(m_lib.resolve("RimeSelectSchema"));

    return m_fnSetup && m_fnInitialize && m_fnCreateSession && m_fnProcessKey &&
           m_fnGetContext && m_fnFreeContext && m_fnGetCommit && m_fnFreeCommit;
}

bool RimeEngine::processKey(int keyCode, int mask) {
    if (m_isLoaded && m_sessionId && m_fnProcessKey) {
        int res = m_fnProcessKey(m_sessionId, keyCode, mask);
        updateState();
        return res != 0;
    }

    // Simulation logic
    if (m_asciiMode) {
        if (keyCode >= 0x20 && keyCode <= 0x7e) {
            emit committed(QString(QChar(keyCode)));
            return true;
        }
        return false;
    }

    if (keyCode == RIME_KEY_BACKSPACE) {
        if (!m_simInput.isEmpty()) {
            m_simInput.chop(1);
            m_simHighlight = 0;
            updateState();
            return true;
        }
        return false;
    } else if (keyCode == RIME_KEY_ESCAPE) {
        if (!m_simInput.isEmpty()) {
            m_simInput.clear();
            m_simHighlight = 0;
            updateState();
            return true;
        }
        return false;
    } else if (keyCode == RIME_KEY_SPACE) {
        if (!m_simInput.isEmpty()) {
            RimeUiState st = getUiState();
            if (!st.candidates.isEmpty() && m_simHighlight < st.candidates.size()) {
                QString commitStr = st.candidates[m_simHighlight].text;
                m_simInput.clear();
                m_simHighlight = 0;
                emit committed(commitStr);
                updateState();
                return true;
            }
        }
        return false;
    } else if (keyCode == RIME_KEY_UP) {
        if (!m_simInput.isEmpty()) {
            if (m_simHighlight > 0) {
                m_simHighlight--;
                updateState();
                return true;
            }
        }
        return false;
    } else if (keyCode == RIME_KEY_DOWN) {
        if (!m_simInput.isEmpty()) {
            RimeUiState st = getUiState();
            if (m_simHighlight < st.candidates.size() - 1) {
                m_simHighlight++;
                updateState();
                return true;
            }
        }
        return false;
    } else if (keyCode >= '1' && keyCode <= '9') {
        int idx = keyCode - '1';
        return selectCandidate(idx);
    } else if (keyCode >= 0x20 && keyCode <= 0x7e) {
        m_simInput.append(QChar(keyCode));
        m_simHighlight = 0;
        updateState();
        return true;
    }

    return false;
}

bool RimeEngine::processChar(QChar ch) {
    return processKey(ch.unicode(), 0);
}

bool RimeEngine::selectCandidate(int index) {
    if (m_isLoaded && m_sessionId && m_fnSelectCandidate) {
        int res = m_fnSelectCandidate(m_sessionId, static_cast<size_t>(index));
        updateState();
        return res != 0;
    }

    // Simulation
    RimeUiState st = getUiState();
    if (index >= 0 && index < st.candidates.size()) {
        QString commitStr = st.candidates[index].text;
        m_simInput.clear();
        m_simHighlight = 0;
        emit committed(commitStr);
        updateState();
        return true;
    }
    return false;
}

RimeUiState RimeEngine::getUiState() {
    RimeUiState state;
    if (m_isLoaded && m_sessionId && m_fnGetContext) {
        RimeContext ctx{};
        ctx.data_size = static_cast<int>(sizeof(RimeContext) - sizeof(int));
        if (m_fnGetContext(m_sessionId, &ctx)) {
            state.isComposing = (ctx.composition.length > 0);
            if (ctx.composition.preedit) {
                state.preedit = QString::fromUtf8(ctx.composition.preedit);
            }
            state.cursorPos = ctx.composition.cursor_pos;
            state.highlightedIndex = ctx.menu.highlighted_candidate_index;
            state.pageNo = ctx.menu.page_no;
            state.isLastPage = (ctx.menu.is_last_page != 0);

            for (int i = 0; i < ctx.menu.num_candidates; ++i) {
                CandidateItem item;
                item.index = i;
                if (ctx.menu.candidates[i].text) {
                    item.text = QString::fromUtf8(ctx.menu.candidates[i].text);
                }
                if (ctx.menu.candidates[i].comment) {
                    item.comment = QString::fromUtf8(ctx.menu.candidates[i].comment);
                }
                state.candidates.append(item);
            }
            m_fnFreeContext(&ctx);

            // Fetch up to 20 candidates across pages via CandidateList iterator for expanded matrix!
            if (state.isComposing && m_fnCandidateListBegin && m_fnCandidateListNext && m_fnCandidateListEnd) {
                RimeCandidateListIterator iter{};
                if (m_fnCandidateListBegin(m_sessionId, &iter)) {
                    QVector<CandidateItem> iterCandidates;
                    while (m_fnCandidateListNext(&iter)) {
                        CandidateItem item;
                        item.index = iter.index;
                        if (iter.candidate.text) {
                            item.text = QString::fromUtf8(iter.candidate.text);
                        }
                        if (iter.candidate.comment) {
                            item.comment = QString::fromUtf8(iter.candidate.comment);
                        }
                        iterCandidates.append(item);
                        if (iterCandidates.size() >= 20) {
                            break;
                        }
                    }
                    m_fnCandidateListEnd(&iter);
                    if (iterCandidates.size() > state.candidates.size()) {
                        state.candidates = iterCandidates;
                    }
                }
            }
        }
        return state;
    }

    // Simulation mock candidates for common inputs
    if (!m_simInput.isEmpty()) {
        state.isComposing = true;
        state.preedit = m_simInput;
        state.cursorPos = m_simInput.length();
        state.highlightedIndex = m_simHighlight;
        state.pageNo = 0;
        state.isLastPage = true;

        if (m_simInput == QStringLiteral("nihao")) {
            const QStringList wordsSimp = {
                QStringLiteral("你好"), QStringLiteral("拟好"), QStringLiteral("泥好"), QStringLiteral("你好吗"), QStringLiteral("你好啊"),
                QStringLiteral("逆号"), QStringLiteral("拟豪"), QStringLiteral("妮好"), QStringLiteral("你壕"), QStringLiteral("拟稿"),
                QStringLiteral("泥濠"), QStringLiteral("霓好"), QStringLiteral("腻好"), QStringLiteral("你号"), QStringLiteral("倪豪"),
                QStringLiteral("昵好"), QStringLiteral("猊好"), QStringLiteral("鲵好"), QStringLiteral("溺好"), QStringLiteral("尼好")
            };
            const QStringList wordsTrad = {
                QStringLiteral("你好"), QStringLiteral("擬好"), QStringLiteral("泥好"), QStringLiteral("你好嗎"), QStringLiteral("你好啊"),
                QStringLiteral("逆號"), QStringLiteral("擬豪"), QStringLiteral("妮好"), QStringLiteral("你壕"), QStringLiteral("擬稿"),
                QStringLiteral("泥濠"), QStringLiteral("霓好"), QStringLiteral("膩好"), QStringLiteral("你號"), QStringLiteral("倪豪"),
                QStringLiteral("暱好"), QStringLiteral("猊好"), QStringLiteral("鯢好"), QStringLiteral("溺好"), QStringLiteral("尼好")
            };
            const auto& list = m_simplified ? wordsSimp : wordsTrad;
            for (int i = 0; i < list.size(); ++i) {
                state.candidates.append({i, list[i], QStringLiteral("nh")});
            }
        } else if (m_simInput == QStringLiteral("fluent")) {
            const QStringList fluentWords = {
                QStringLiteral("Fluent-Qt"), QStringLiteral("流畅"), QStringLiteral("流利"), QStringLiteral("fluent"), QStringLiteral("FluentUI"),
                QStringLiteral("流传"), QStringLiteral("流体"), QStringLiteral("流程"), QStringLiteral("流向"), QStringLiteral("流域"),
                QStringLiteral("流量"), QStringLiteral("流浪"), QStringLiteral("流动"), QStringLiteral("流通"), QStringLiteral("流行"),
                QStringLiteral("流水"), QStringLiteral("流失"), QStringLiteral("流言"), QStringLiteral("流览"), QStringLiteral("流星")
            };
            for (int i = 0; i < fluentWords.size(); ++i) {
                state.candidates.append({i, fluentWords[i], QStringLiteral("fl")});
            }
        } else {
            state.candidates.append({0, m_simInput, QStringLiteral("raw")});
            const QString baseWord = m_simplified ? QStringLiteral("候选词") : QStringLiteral("候選詞");
            for (int i = 1; i < 20; ++i) {
                state.candidates.append({i, QStringLiteral("%1 %2").arg(baseWord).arg(i), QString::number(i)});
            }
        }
    }
    return state;
}

QString RimeEngine::getCommitText() {
    if (m_isLoaded && m_sessionId && m_fnGetCommit) {
        RimeCommit commit{};
        commit.data_size = static_cast<int>(sizeof(RimeCommit) - sizeof(int));
        if (m_fnGetCommit(m_sessionId, &commit)) {
            QString text;
            if (commit.text) {
                text = QString::fromUtf8(commit.text);
            }
            m_fnFreeCommit(&commit);
            return text;
        }
    }
    return QString();
}

void RimeEngine::clear() {
    if (m_isLoaded && m_sessionId) {
        processKey(RIME_KEY_ESCAPE, 0);
    }
    m_simInput.clear();
    m_simHighlight = 0;
    updateState();
}

bool RimeEngine::deploy() {
    if (m_isLoaded && m_fnDeployWorkspace) {
        return m_fnDeployWorkspace() != 0;
    }
    return true;
}

void RimeEngine::updateState() {
    QString commitText = getCommitText();
    if (!commitText.isEmpty()) {
        emit committed(commitText);
    }
    RimeUiState st = getUiState();
    emit stateChanged(st);
}

void RimeEngine::commitDirectText(const QString& text) {
    if (!text.isEmpty()) {
        clear();
        emit committed(text);
    }
}

void RimeEngine::setAsciiMode(bool ascii) {
    m_asciiMode = ascii;
    if (m_isLoaded && m_sessionId && m_fnSetOption) {
        m_fnSetOption(m_sessionId, "ascii_mode", ascii ? 1 : 0);
    }
    if (ascii) {
        clear();
    }
    updateState();
}

void RimeEngine::setFullShape(bool full) {
    m_fullShape = full;
    if (m_isLoaded && m_sessionId && m_fnSetOption) {
        m_fnSetOption(m_sessionId, "full_shape", full ? 1 : 0);
    }
    updateState();
}

void RimeEngine::setSimplified(bool simp) {
    m_simplified = simp;
    if (m_isLoaded && m_sessionId && m_fnSetOption) {
        m_fnSetOption(m_sessionId, "simplification", simp ? 1 : 0);
        m_fnSetOption(m_sessionId, "zh_simp", simp ? 1 : 0);
    }
    updateState();
}

bool RimeEngine::selectSchema(const QString& schemaId) {
    m_currentSchema = schemaId;
    if (m_isLoaded && m_sessionId && m_fnSelectSchema) {
        int res = m_fnSelectSchema(m_sessionId, schemaId.toUtf8().constData());
        updateState();
        return res != 0;
    }
    updateState();
    return true;
}


