#include "common.h"

#include <getopt.h>
#include <proto/exec.h>

typedef enum {
    PM_All,
    PM_Live,
} ProgramMode;

typedef struct  {
    const char* base_path;
    ProgramMode program_mode;
} Arguments;

static Status args_parse(Arguments* args, int argc, char *argv[]);

struct Library* OpenURLBase;

int main(int argc, char *argv[]) {
    TRY
#ifdef FORTIFY
    Fortify_EnterScope();
#endif

    Arguments args = {0};

    ASSERT(OpenURLBase = OpenLibrary("openurl.library", 0));
    CHECK(args_parse(&args, argc, argv));
    CHECK(html_init(args.base_path));
    CHECK(page_init());

    if (args.program_mode == PM_All) {
        CHECK(page_build_all(args.base_path));
    } else {
        CHECK(page_build_live(args.base_path));
    }

    FINALLY
    page_fini();
    html_fini();
    CloseLibrary(OpenURLBase);

#ifdef FORTIFY
    Fortify_LeaveScope();
#endif

    return 0;
}

static Status args_parse(Arguments* args, int argc, char *argv[]) {
    TRY
    bool opt_all = false;
    bool opt_live = false;

    struct option long_opts[] = {
        {"all",     no_argument,       NULL, 'a'},
        {"basedir", required_argument, NULL, 'b'},
        {"help",    no_argument,       NULL, 'h'},
        {"live",    no_argument,       NULL, 'l'},
        {NULL,      0,                 NULL, 0  }
    };

    for (int short_opt; (short_opt = getopt_long(argc, argv, "ab:hl", long_opts, NULL)) != -1;) {
        switch (short_opt) {
        case 'a':
            opt_all = true;
            break;
        case 'b':
            args->base_path = optarg;
            break;
        case 'l':
            opt_live = true;
            break;
        case 'h':
            fprintf(stderr, "Usage: AGP -b BASEDIR [OPTION]...\n\n");
            fprintf(stderr, "  -a, --all      Find and build all index.md files under BASEDIR\n");
            fprintf(stderr, "  -b, --basedir  Top-level website directory\n");
            fprintf(stderr, "  -h, --help     Show this help message\n");
            fprintf(stderr, "  -l, --live     Find index.md opened in TextEdit, build and reload HTML\n");
        case ':':
        case '?':
            THROW(StatusQuit);
        }
    }

    ASSERT(args->base_path, "Option --basedir is required");
    ASSERT(opt_all != opt_live, "Option --all or --live is required");

    args->program_mode = opt_all ? PM_All : PM_Live;

    FINALLY RETURN;
}
