#include <cstring>
#include <ctime>
#include <switch.h>
#include "scope_guard.hpp"
#include "internal.h"
#include <arm_neon.h>

extern "C" {
extern u8 __tls_start[];

u32 __nx_fs_num_sessions = 1;

void __libnx_init(void*, Handle, void*);
void __libnx_exit(int) {}

/* Exception handling. */
alignas(16) u8 __nx_exception_stack[0];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void s_printf(char *out_buf, const char *fmt, ...);
}

#define R_ABORT_UNLESS(res_expr)                 \
    ({                                           \
        const auto _tmp_r_abort_rc = (res_expr); \
        if (R_FAILED(_tmp_r_abort_rc)) {         \
            diagAbortWithResult(_tmp_r_abort_rc);\
        }                                        \
    })

void __libnx_init(void*, Handle main_thread, void*) {
    // Initialize thread vars for the main thread
    ThreadVars* tv = getThreadVars();
    tv->magic      = THREADVARS_MAGIC;
    tv->thread_ptr = NULL;
    tv->reent      = _impure_ptr;
    tv->tls_tp     = __tls_start-2*sizeof(void*); // subtract size of Thread Control Block (TCB)
    tv->handle     = main_thread;

    while ((armGetSystemTick() / armGetSystemTickFreq()) < 10)
        svcSleepThread(1'000'000'000);

    R_ABORT_UNLESS(smInitialize());
    {
        R_ABORT_UNLESS(setsysInitialize());
        {
            SetSysFirmwareVersion version;
            R_ABORT_UNLESS(setsysGetFirmwareVersion(&version));
            hosversionSet(MAKEHOSVERSION(version.major, version.minor, version.micro));
            setsysExit();
        }

        if (hosversionBefore(3, 0, 0))
            svcExitProcess();

        R_ABORT_UNLESS(hidsysInitialize());
        R_ABORT_UNLESS(capsscInitialize());
        R_ABORT_UNLESS(fsInitialize());
    }
    smExit();
}

struct bmp_t {
    u16 magic;
    u32 size;
    u32 rsvd;
    u32 data_off;
    u32 hdr_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 pxl_bits;
    u32 comp;
    u32 img_size;
    u32 res_h;
    u32 res_v;
    u64 rsvd2;
} __attribute__((packed));

constexpr const u32 InComponents  = 4;
constexpr const u32 OutComponents = 3;
constexpr const u32 Width         = 1280;
constexpr const u32 Height        = 720;

constexpr const u32 InLineSize  = Width * InComponents;
constexpr const u32 OutLineSize = Width * OutComponents;

constexpr const u32 InSize   = InLineSize * Height;
constexpr const u32 OutSize  = OutLineSize * Height;
constexpr const u32 FileSize = OutSize + 0x36;

constexpr const u64 divider       = 10;
constexpr const u32 InBufferSize  = InLineSize * divider;
constexpr const u32 OutBufferSize = OutLineSize * divider;

static_assert((Height % divider) == 0);

constexpr const bmp_t bmp = {
    .magic    = 0x4D42,
    .size     = FileSize,
    .rsvd     = 0,
    .data_off = 0x36,
    .hdr_size = 40,
    .width    = Width,
    .height   = Height,
    .planes   = 1,
    .pxl_bits = 24,
    .comp     = 0,
    .img_size = OutSize,
    .res_h    = 2834,
    .res_v    = 2834,
    .rsvd2    = 0,
};

[[gnu::aligned(0x40)]]
static u8 in_buffer[InBufferSize];
[[gnu::aligned(0x40)]]
static u8 out_buffer[OutBufferSize];

char path_buffer[FS_MAX_PATH];
char b_path_buffer[FS_MAX_PATH];

#define R_TRY(res_expr)        \
    ({                         \
        auto res = (res_expr); \
        if (R_FAILED(res))     \
            return res;        \
    })

Result Capture() {
    /* Get filesystem handle. */
    FsFileSystem fs;
    R_TRY(fsOpenImageDirectoryFileSystem(&fs, FsImageDirectoryId_Sd));
    ScopeGuard fs_guard([&fs] { fsFsClose(&fs); });

    /* Create bitmap directory. */
    std::strcpy(path_buffer, "/Bitmaps/");
    Result rc = fsFsCreateDirectory(&fs, path_buffer);

    /* Path already exists. */
    if (R_FAILED(rc) && rc != 0x402)
        return rc;

    /* Make unique path. */
    std::strcpy(path_buffer, "/Bitmaps/tmp.bmp");

    /* Create file. */
    R_TRY(fsFsCreateFile(&fs, path_buffer, FileSize, 0));
    ScopeGuard rm_guard([&fs] { fsFsDeleteFile(&fs, path_buffer); });

    /* Open file. */
    FsFile file;
    R_TRY(fsFsOpenFile(&fs, path_buffer, FsOpenMode_Write, &file));
    ScopeGuard file_guard([&file] { fsFileClose(&file); });

    /* Write bitmap header. */
    off_t offset = 0;
    fsFileWrite(&file, 0, &bmp, 54, FsWriteOption_None);
    offset += 54;

    u64 written = 0;
    for (int y = (Height / divider) - 1; y >= 0; y--) {
        /* Read raw image data */
        R_TRY(capsscReadRawScreenShotReadStream(&written, in_buffer, sizeof(in_buffer), y * sizeof(in_buffer)));

        /* Resample buffer bottom up. */
        for (int div_y = (divider - 1); div_y >= 0; div_y--) {
            u8 *out = out_buffer + (div_y * OutLineSize);
            u8 *in  = in_buffer + ((divider - div_y - 1) * InLineSize);

            /* BGRX to RGB bitmap */
            for (u32 x = 0; x < Width; x += 16, in += 16 * InComponents, out += 16 * OutComponents) {
                uint8x16x4_t bgra = vld4q_u8(in);
                uint8x16x3_t rgb = { bgra.val[2], bgra.val[1], bgra.val[0] };
                vst3q_u8(out, rgb);
            }
        }

        /* Write to file. */
        R_TRY(fsFileWrite(&file, offset, out_buffer, sizeof(out_buffer), FsWriteOption_None));
        offset += sizeof(out_buffer);
    }

    rm_guard.Cancel();
    file_guard.Invoke();

    /* I don't care if any of this fails. */
    FsTimeStampRaw timestamp;
    if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(&fs, path_buffer, &timestamp))) {
        time_t ts = timestamp.created;
        tm _t;
        tm *t = gmtime_r(&ts, &_t);
        s_printf(b_path_buffer, "/Bitmaps/%d-%02d-%02d_%02d-%02d-%02d.bmp",
                      t->tm_year + 1900,
                      t->tm_mon + 1,
                      t->tm_mday,
                      t->tm_hour,
                      t->tm_min,
                      t->tm_sec);
        fsFsRenameFile(&fs, path_buffer, b_path_buffer);
    }

    return 0;
}

int main() {
    bool held = false;

    u64 start_tick = 0;

    /* Obtain capture button event. */
    Event event;
    R_ABORT_UNLESS(hidsysAcquireCaptureButtonEventHandle(&event, false));
    eventClear(&event);

    /* Loop forever. */
    while (true) {
        if (R_SUCCEEDED(eventWait(&event, 17'000'000))) {
            eventClear(&event);
            if (!held) {
                /* Capture screen in VI. */
                if (R_SUCCEEDED(capsscOpenRawScreenShotReadStream(nullptr, nullptr, nullptr, ViLayerStack_Default, 100'000'000))) {
                    held       = true;
                    start_tick = armGetSystemTick();
                }
            } else if (start_tick != 0) {
                /* Capture bitmap in file. */
                Capture();
                /* Discard capture. */
                capsscCloseRawScreenShotReadStream();
                start_tick = 0;
                held       = false;
            } else {
                held = false;
            }
        } else {
            if (start_tick != 0) {
                /* If held for more than half a second we discard the capture. */
                if (armTicksToNs(armGetSystemTick() - start_tick) > 500'000'000) {
                    capsscCloseRawScreenShotReadStream();
                    start_tick = 0;
                    held       = false;
                }
            }
        }
    }

    /* Unreachable lol */
}
