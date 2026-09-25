import ctypes

dll = ctypes.CDLL(r"C:\Program Files\Rime\weasel-0.17.4\rime.dll")

funcs = [
    "RimeCandidateListBegin",
    "RimeCandidateListNext",
    "RimeCandidateListEnd",
    "RimeCandidateListFromIndex",
    "RimeSelectCandidate",
    "RimeSelectCandidateOnCurrentPage",
    "RimeGetContext",
    "RimeFreeContext"
]

for f in funcs:
    if hasattr(dll, f):
        print(f"FOUND: {f}")
    else:
        print(f"MISSING: {f}")
