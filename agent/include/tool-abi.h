// tool_abi.h
//
// Day la SCAFFOLD DUY NHAT ma agent cong bo ra ngoai. Bat ky du an tool nao
// (nam o repo rieng, do ai viet cung duoc) chi can #include file nay va
// export dung 1 symbol ten "agent_tool_entry" la co the duoc agent nap vao.
//
// Dung C ABI thuan (khong dua thang C++ class/vtable qua ranh gioi .so),
// vi ABI cua C++ (ten ham mangling, layout vtable, exception...) KHONG on dinh
// giua cac phien ban compiler/libstdc++ khac nhau. Neu agent bien dich bang
// GCC 13 va tool build bang GCC 11 hoac Clang, mot C++ interface truc tiep co
// the crash ngay khi goi virtual function. C struct + function pointer thi
// on dinh tuyet doi vi day la ABI muc he dieu hanh, khong phai ABI compiler.

#ifndef AGENT_TOOL_ABI_H
#define AGENT_TOOL_ABI_H

#ifdef __cplusplus
extern "C" {
#endif

// Tang version nay MOI KHI thay doi struct ben duoi theo huong khong tuong
// thich nguoc. Agent se tu choi nap tool co abi_version khac gia tri nay.
#define AGENT_TOOL_ABI_VERSION 1

typedef struct AgentToolInterface {
    const char* (*name)(void);
    const char* (*description)(void);
    const char* (*parameters_schema)(void);  // JSON schema dang string

    // args_json: do AGENT cap phat va so huu, plugin chi doc, khong duoc free.
    // Tra ve: JSON string do PLUGIN cap phat bang malloc/strdup.
    // Agent PHAI goi free_result() de giai phong - KHONG duoc goi free()
    // truc tiep tu phia agent, vi agent va plugin co the link 2 allocator
    // khac nhau (vi du plugin dung tcmalloc rieng).
    char* (*execute)(const char* args_json);
    void (*free_result)(char* ptr);
} AgentToolInterface;

typedef struct AgentToolPluginEntry {
    int abi_version;  // PHAI = AGENT_TOOL_ABI_VERSION luc plugin duoc bien dich
    const AgentToolInterface* (*get_interface)(void);
} AgentToolPluginEntry;

// Ten symbol duy nhat ma moi file .so PHAI export voi extern "C".
#define AGENT_TOOL_ENTRY_SYMBOL "agent_tool_entry"

#ifdef __cplusplus
}
#endif

#endif  // AGENT_TOOL_ABI_H
