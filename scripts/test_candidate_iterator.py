import ctypes

dll = ctypes.CDLL(r"C:\Program Files\Rime\weasel-0.17.4\rime.dll")

class RimeCandidate(ctypes.Structure):
    _fields_ = [
        ("text", ctypes.c_char_p),
        ("comment", ctypes.c_char_p),
        ("reserved", ctypes.c_void_p),
    ]

class RimeCandidateListIterator(ctypes.Structure):
    _fields_ = [
        ("ptr", ctypes.c_void_p),
        ("index", ctypes.c_int),
        ("candidate", RimeCandidate),
    ]

class RimeTraits(ctypes.Structure):
    _fields_ = [
        ("data_size", ctypes.c_int),
        ("shared_data_dir", ctypes.c_char_p),
        ("user_data_dir", ctypes.c_char_p),
        ("distribution_name", ctypes.c_char_p),
        ("distribution_code_name", ctypes.c_char_p),
        ("distribution_version", ctypes.c_char_p),
        ("app_name", ctypes.c_char_p),
        ("modules", ctypes.POINTER(ctypes.c_char_p)),
        ("min_log_level", ctypes.c_int),
        ("log_dir", ctypes.c_char_p),
        ("prebuilt_data_dir", ctypes.c_char_p),
        ("staging_dir", ctypes.c_char_p),
    ]

dll.RimeSetup.argtypes = [ctypes.POINTER(RimeTraits)]
dll.RimeInitialize.argtypes = [ctypes.POINTER(RimeTraits)]
dll.RimeCreateSession.restype = ctypes.c_uint64
dll.RimeDestroySession.argtypes = [ctypes.c_uint64]
dll.RimeProcessKey.argtypes = [ctypes.c_uint64, ctypes.c_int, ctypes.c_int]
dll.RimeProcessKey.restype = ctypes.c_int
dll.RimeCandidateListBegin.argtypes = [ctypes.c_uint64, ctypes.POINTER(RimeCandidateListIterator)]
dll.RimeCandidateListBegin.restype = ctypes.c_int
dll.RimeCandidateListNext.argtypes = [ctypes.POINTER(RimeCandidateListIterator)]
dll.RimeCandidateListNext.restype = ctypes.c_int
dll.RimeCandidateListEnd.argtypes = [ctypes.POINTER(RimeCandidateListIterator)]

traits = RimeTraits()
traits.data_size = ctypes.sizeof(RimeTraits) - ctypes.sizeof(ctypes.c_int)
traits.shared_data_dir = b"C:/Program Files/Rime/weasel-0.17.4/data"
traits.user_data_dir = b"C:/Users/zheng/AppData/Roaming/Rime"
traits.app_name = b"rime.test"

dll.RimeSetup(ctypes.byref(traits))
dll.RimeInitialize(ctypes.byref(traits))
dll.RimeStartMaintenance(0)
session_id = dll.RimeCreateSession()
print(f"Session ID: {session_id}")

if session_id:
    # Type 'nihao'
    for ch in b"nihao":
        dll.RimeProcessKey(session_id, ch, 0)

    iterator = RimeCandidateListIterator()
    res = dll.RimeCandidateListBegin(session_id, ctypes.byref(iterator))
    print(f"CandidateListBegin result: {res}")

    candidates = []
    if res:
        while dll.RimeCandidateListNext(ctypes.byref(iterator)):
            text = iterator.candidate.text.decode('utf-8') if iterator.candidate.text else ""
            comment = iterator.candidate.comment.decode('utf-8') if iterator.candidate.comment else ""
            candidates.append((iterator.index, text, comment))
            if len(candidates) >= 20:
                break
        dll.RimeCandidateListEnd(ctypes.byref(iterator))

    print(f"Found {len(candidates)} candidates via Iterator:")
    for idx, text, comment in candidates:
        print(f"  [{idx}] {text} {comment}")

    dll.RimeDestroySession(session_id)
