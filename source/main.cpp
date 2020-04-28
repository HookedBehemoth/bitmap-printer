#include <cstdio>
#include <switch.h>

extern "C" {
extern u32 __start__;

u32 __nx_applet_type = AppletType_None;

/* TODO: Reduce this to effectively 0 */
#define INNER_HEAP_SIZE 0x4000
size_t nx_inner_heap_size = INNER_HEAP_SIZE;
char nx_inner_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void);
void __appInit(void);
void __appExit(void);

/* Exception handling. */
alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
void __libnx_exception_handler(ThreadExceptionDump *ctx);
}

void __libnx_initheap(void) {
    void *addr = nx_inner_heap;
    size_t size = nx_inner_heap_size;

    // Newlib
    extern char *fake_heap_start;
    extern char *fake_heap_end;

    fake_heap_start = (char *)addr;
    fake_heap_end = (char *)addr + size;
}

#define R_ABORT_UNLESS(res_expr)                 \
    ({                                           \
        const auto _tmp_r_abort_rc = (res_expr); \
        if (R_FAILED(_tmp_r_abort_rc)) {         \
            fatalThrow(_tmp_r_abort_rc);         \
        }                                        \
    })

bool initialized = false;

void __attribute__((weak)) __appInit(void) {
    R_ABORT_UNLESS(smInitialize());
    {
        R_ABORT_UNLESS(setsysInitialize());
        {
            SetSysFirmwareVersion version;
            R_ABORT_UNLESS(setsysGetFirmwareVersion(&version));
            hosversionSet(MAKEHOSVERSION(version.major, version.minor, version.micro));
            setsysExit();
        }

        if (hosversionAtLeast(3,0,0)) {
            R_ABORT_UNLESS(hidsysInitialize());
            R_ABORT_UNLESS(capsscInitialize());
            R_ABORT_UNLESS(fsInitialize());
            initialized = true;
        }
    }
    smExit();
}

void __attribute__((weak)) __appExit(void) {
    if (initialized) {
        fsExit();
        capsscExit();
        hidsysExit();
    }
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

constexpr const u32 InComponents = 4;
constexpr const u32 OutComponents = 3;
constexpr const u32 Width = 1280;
constexpr const u32 Height = 720;

constexpr const u32 InLineSize = Width * InComponents;
constexpr const u32 OutLineSize = Width * OutComponents;

constexpr const u32 InSize = InLineSize * Height;
constexpr const u32 OutSize = OutLineSize * Height;
constexpr const u32 FileSize = OutSize + 0x36;

constexpr const u64 divider = 20;
constexpr const u32 InBufferSize = InLineSize * divider;
constexpr const u32 OutBufferSize = OutLineSize * divider;

static_assert((Height % divider) == 0);

constexpr const bmp_t bmp = {
    .magic = 0x4D42,
    .size = FileSize,
    .rsvd = 0,
    .data_off = 0x36,
    .hdr_size = 40,
    .width = Width,
    .height = Height,
    .planes = 1,
    .pxl_bits = 24,
    .comp = 0,
    .img_size = OutSize,
    .res_h = 2834,
    .res_v = 2834,
    .rsvd2 = 0,
};

static u8 in_buffer[InBufferSize];
static u8 out_buffer[OutBufferSize];

bool should_run;
bool request_capture;

char path_buffer[FS_MAX_PATH];

#include <functional>
class ScopeGuard {
    std::function<void()> m_f;

  public:
    ScopeGuard(std::function<void()> f) : m_f(f) {}
    ~ScopeGuard() {
        if (m_f != nullptr)
            m_f();
    }
    void Cancel() { m_f = nullptr; };
};

#define R_CONTINUE(res_expr)   \
    ({                         \
        auto res = (res_expr); \
        if (R_FAILED(res))     \
            continue;          \
    })

void CaptureFunction(void *) {
    /* Get filesystem handle. */
    FsFileSystem sdmc;
    if (R_FAILED(fsOpenSdCardFileSystem(&sdmc))) {
        should_run = false;
        return;
    }

    while (should_run) {
        /* Sleep thread until prompted */
        if (!request_capture) {
            svcSleepThread(100'000'000);
            continue;
        }

        ScopeGuard flag_guard([] { request_capture = false; });

        /* Open read stream */
        u64 size, width, height;
        R_CONTINUE(capsscOpenRawScreenShotReadStream(&size, &width, &height, ViLayerStack_Default, 100'000'000));
        ScopeGuard stream_guard([] { capsscCloseRawScreenShotReadStream(); });

        /* Notify capture. */
        /* TODO! */
        /* ScopeGuard notify_guard([] { capsscNotifyTakingScreenShotFailed(); }) */

        /* Make unique path. */
        u64 tick = armGetSystemTick();
        std::snprintf(path_buffer, 0x20, "/%ld.bmp", tick);

        /* Create file. */
        R_CONTINUE(fsFsCreateFile(&sdmc, path_buffer, FileSize, 0));
        ScopeGuard rm_guard([&sdmc] { fsFsDeleteFile(&sdmc, path_buffer); });

        FsFile file;
        R_CONTINUE(fsFsOpenFile(&sdmc, path_buffer, FsOpenMode_Write, &file));
        ScopeGuard file_guard([&file] { fsFileClose(&file); });

        off_t offset = 0;
        fsFileWrite(&file, 0, &bmp, 54, FsWriteOption_None);
        offset += 54;

        u64 written = 0;
        for (int y = (Height / divider) - 1; y >= 0; y--) {
            /* Read raw image data */
            R_CONTINUE(capsscReadRawScreenShotReadStream(&written, in_buffer, sizeof(in_buffer), y * sizeof(in_buffer)));

            /* Resample buffer bottom up. */
            for (int div_y = (divider - 1); div_y >= 0; div_y--) {
                u8 *out = out_buffer + (div_y * OutLineSize);
                u8 *in = in_buffer + ((divider - div_y - 1) * InLineSize);

                /* RGBX to RGB bitmap */
                for (u32 x = 0; x < Width; x++) {
                    out[0] = in[2];
                    out[1] = in[1];
                    out[2] = in[0];
                    in += 4;
                    out += 3;
                }
            }

            /* Write to file. */
            fsFileWrite(&file, offset, out_buffer, sizeof(out_buffer), FsWriteOption_None);
            offset += sizeof(out_buffer);
        }

        rm_guard.Cancel();
        /* notify_guard.Cancel(); */
    }
    /* Close filesystem. */
    fsFsClose(&sdmc);
}

int main(int argc, char *argv[]) {
    /* Quietly exit on unsupported versions. */
    if (!initialized)
        return 0;

    should_run = true;
    request_capture = false;

    bool held = false;

    Thread capture_thread;
    R_ABORT_UNLESS(threadCreate(&capture_thread, CaptureFunction, nullptr, nullptr, 0x1000, 0x20, -2));
    R_ABORT_UNLESS(threadStart(&capture_thread));

    Event event;
    R_ABORT_UNLESS(hidsysAcquireCaptureButtonEventHandle(&event));
    eventClear(&event);

    while (true) {
        if (R_SUCCEEDED(eventWait(&event, UINT64_MAX))) {
            held = !held;
            if (held) {
                request_capture = true;
            }
            eventClear(&event);
        } else {
            /* Something went horribly wrong! */
            break;
        }
    }
    eventClose(&event);
    should_run = false;

    R_ABORT_UNLESS(threadWaitForExit(&capture_thread));
    R_ABORT_UNLESS(threadClose(&capture_thread));
    return 0;
}
