#include "common.h"

#include <proto/exec.h>
#include <proto/rexxsyslib.h>

#define TEXTEDIT_PORT_PREFIX "TEXTEDIT."

static Status find_textedit_ports(char*** port_names_p);
static Status send_message(char** result_p, const char* port_name,
    struct MsgPort* reply_port, struct RexxMsg* message, char *command);

Status rexx_get_live_paths(char*** paths_p) {
    TRY
    struct MsgPort* reply_port = NULL;
    struct RexxMsg* message = NULL;
    char** port_names = NULL;
    char* file_name = NULL;

    ASSERT(reply_port = CreateMsgPort());
    ASSERT(message = CreateRexxMsg(reply_port, NULL, NULL));
    CHECK(vector_new(paths_p, sizeof(char*), 0));
    CHECK(vector_new(&port_names, sizeof(char*), 0));

    CHECK(find_textedit_ports(&port_names));

    vector_foreach(port_names, char*, port_name_p) {
        CHECK(send_message(&file_name, *port_name_p, reply_port, message, "GETATTR PROJECT FILENAME"))

        if (string_endswith(file_name, "index.md")) {
            CHECK(send_message(NULL, *port_name_p, reply_port, message, "SAVE"))
            CHECK(vector_append(paths_p, 1, &file_name));
            file_name = NULL;
        } else {
            string_free(&file_name);
        }
    }

    FINALLY
    string_free(&file_name);

    if (port_names) {
        vector_foreach(port_names, char*, port_name_p) {
            string_free(port_name_p);
        }

        vector_free(&port_names);
    }

    if (message) {
        DeleteRexxMsg(message);
    }

    DeleteMsgPort(reply_port);

    RETURN;
}

static Status find_textedit_ports(char*** port_names_p) {
    TRY
    Forbid();

    for (struct Node* node = SysBase->PortList.lh_Head; node->ln_Succ; node = node->ln_Succ) {
        if (node->ln_Name && (strncmp(node->ln_Name, TEXTEDIT_PORT_PREFIX, strlen(TEXTEDIT_PORT_PREFIX)) == 0)) {
            CHECK(vector_append(port_names_p, 1, NULL));
            CHECK(string_clone(&(*port_names_p)[vector_length(*port_names_p) - 1], node->ln_Name));
        }
    }

    FINALLY
    Permit();

    RETURN;
}

static Status send_message(char** result_p, const char* port_name,
    struct MsgPort* reply_port, struct RexxMsg* message, char *command)
{
    TRY
    message->rm_Args[0] = command;
    message->rm_Action = RXCOMM | RXFF_RESULT;
    ASSERT(FillRexxMsg(message, 1, 0));

    Forbid();

    struct MsgPort* port = FindPort(port_name);

    if (port) {
        PutMsg(port, &message->rm_Node);
    }

    Permit();

    if (port) {
        WaitPort(reply_port);
        GetMsg(reply_port);

        ASSERT(message->rm_Result1 == 0);

        if (result_p) {
            ASSERT(message->rm_Result2);
            CHECK(string_clone(result_p, (char*)message->rm_Result2));
        }
    }

    FINALLY
    if (message->rm_Result1 == 0 && message->rm_Result2) {
        DeleteArgstring((UBYTE *)message->rm_Result2);
        message->rm_Result2 = 0;
    }

    ClearRexxMsg(message, 1);

    RETURN;
}
