#include <stdint.h>
#include <stdio.h>
#include "state_machine.h"
#include "rtthread.h"

/* This simple example checks keyboad input against the two allowed strings
 * "han" and "hin". If an unrecognised character is read, a group state will
 * handle this by printing a message and returning to the idle state. If the
 * character '!' is encountered, a "reset" message is printed, and the group
 * state's entry state will be entered (the idle state).
 *
 *                   print 'reset'
 *       o      +---------------------+
 *       |      |                     | '!'
 *       |      v     group state     |
 * +-----v-----------------------------------+----+
 * |  +------+  'h'  +---+  'a'  +---+  'n'      |
 * +->| idle | ----> | h | ----> | a | ---------+ |
 * |  +------+       +---+\      +---+          | |
 * |   ^ ^ ^               \'i'  +---+  'n'    | |
 * |   | | |                \--> | i | ------+  | |
 * |   | | |                     +---+       |  | |
 * +---|-|-|----------------+----------------|--|-+
 *     | | |                |                |  |
 *     | | |                | '[^hai!\n]'    |  |
 *     | | | print unrecog. |                |  |
 *     | | +----------------+   print 'hi'   |  |
 *     | +-----------------------------------+  |
 *     |               print 'ha'               |
 *     +----------------------------------------+
 */

/* Types of events */
enum event_type
{
    EVENT_KEYBOARD,
};

/* Compare keyboard character from transition's condition variable against
 * data in event. */
static bool Eventkey_guard(void *ch, struct event *event);

static void print_msg_recognised_char(void *state_data, struct event *event);
static void print_msg_unrecognised_char(void *oldstate_data, struct event *event,
                                        void *state_new_data);
static void print_msg_reset(void *oldstate_data, struct event *event,
                            void *state_new_data);
static void print_msg_hi(void *oldstate_data, struct event *event,
                         void *state_new_data);
static void print_msg_ha(void *oldstate_data, struct event *event,
                         void *state_new_data);
static void print_msg_err(void *state_data, struct event *event);
static void print_msg_enter(void *state_data, struct event *event);
static void print_msg_exit(void *state_data, struct event *event);

/* Forward declaration of states so that they can be defined in an logical
 * order: */
static struct state state_charsgroup_check, state_idle, state_h, state_i, state_a;

// 以下所有状态（除了错误状态）都是该组状态的子状态。 这样，任何无法识别的字符都将由该状态的转换处理，从而无需将相同的转换添加到所有子状态。
static struct state state_charsgroup_check = {
    .state_parent = NULL,
    // 定义进入状态是为了证明进入该组状态的“重置”转换将被“重定向”到“空闲”状态（转换当然可以直接进入“空闲”状态）
    .state_entry = &state_idle,
    .transitions = (struct transition[]){
        {
            EVENT_KEYBOARD,        // event_type: 事件类型
            (void *)(intptr_t)'!', // condition:  转换条件
            &Eventkey_guard,       // guard:      将上面两个值代入，检查是否满足转换条件
            &print_msg_reset,      // action
            &state_idle,           // state_next
        },
        {
            EVENT_KEYBOARD,
            NULL,
            NULL,
            &print_msg_unrecognised_char,
            &state_idle,
        },
    },
    .transition_nums = 2,
    .data = "group",
    .action_entry = &print_msg_enter, //
    .action_exti = &print_msg_exit,   // 退出当前状态的操作
};

// idle״̬
static struct state state_idle = {
    .state_parent = &state_charsgroup_check,
    .state_entry = NULL,
    .transitions = (struct transition[]){
        {EVENT_KEYBOARD, (void *)(intptr_t)'h', &Eventkey_guard, NULL, &state_h},
    },
    .transition_nums = 1,
    .data = "idle",
    .action_entry = &print_msg_enter,
    .action_exti = &print_msg_exit,
};

// h：等待a和i事件参数
static struct state state_h = {
    .state_parent = &state_charsgroup_check,
    .state_entry = NULL,
    .transitions = (struct transition[]){
        {EVENT_KEYBOARD, (void *)(intptr_t)'a', &Eventkey_guard, NULL, &state_a},
        {EVENT_KEYBOARD, (void *)(intptr_t)'i', &Eventkey_guard, NULL, &state_i},
    },
    .transition_nums = 2,
    .data = "H",
    .action_entry = &print_msg_recognised_char,
    .action_exti = &print_msg_exit,
};

// i：等待n事件参数
static struct state state_i = {
    .state_parent = &state_charsgroup_check,
    .state_entry = NULL,
    .transitions = (struct transition[]){
        {EVENT_KEYBOARD, (void *)(intptr_t)'n', &Eventkey_guard, &print_msg_hi, &state_idle}},
    .transition_nums = 1,
    .data = "I",
    .action_entry = &print_msg_recognised_char,
    .action_exti = &print_msg_exit,
};

// a：等待n事件参数
static struct state state_a = {
    .state_parent = &state_charsgroup_check,
    .state_entry = NULL,
    .transitions = (struct transition[]){
        {EVENT_KEYBOARD, (void *)(intptr_t)'n', &Eventkey_guard, &print_msg_ha, &state_idle}},
    .transition_nums = 1,
    .data = "A",
    .action_entry = &print_msg_recognised_char,
    .action_exti = &print_msg_exit};

// error
static struct state state_error = {
    .transitions = (struct transition[]){
        {EVENT_KEYBOARD, (void *)(intptr_t)'i', &Eventkey_guard, NULL, &state_i},
    },
    .transition_nums = 1,
    .data = "Error",
    .action_entry = &print_msg_err};

static bool Eventkey_guard(void *ch, struct event *event)
{
    if (event->type != EVENT_KEYBOARD)
    {
        return false;
    }

    // ch = t->conditon
    // t->conditon == event->data
    // ����˵��ǰת�Ƶ�����Ҫ���¼���data�Ǻ�
    return ((intptr_t)ch == (intptr_t)event->data);
}

static void print_msg_recognised_char(void *state_data, struct event *event)
{
    print_msg_enter(state_data, event);
    rt_kprintf("parsed: %c\n", (char)(intptr_t)event->data);
}

static void print_msg_unrecognised_char(void *oldstate_data, struct event *event,
                                        void *state_new_data)
{
    rt_kprintf("unrecognised character: %c\n", (char)(intptr_t)event->data);
}

static void print_msg_reset(void *oldstate_data, struct event *event,
                            void *state_new_data)
{
    rt_kprintf("Resetting\n");
}

static void print_msg_hi(void *oldstate_data, struct event *event,
                         void *state_new_data)
{
    rt_kprintf("Hi!\n");
}

static void print_msg_ha(void *oldstate_data, struct event *event,
                         void *state_new_data)
{
    rt_kprintf("Ha-ha\n");
}

static void print_msg_err(void *state_data, struct event *event)
{
    rt_kprintf("ENTERED ERROR STATE!\n");
}

static void print_msg_enter(void *state_data, struct event *event)
{
    rt_kprintf("Entering %s state\n", (char *)state_data);
}

static void print_msg_exit(void *state_data, struct event *event)
{
    rt_kprintf("Exiting %s state\n", (char *)state_data);
}

rt_mailbox_t mb_key;

static void state_process(void *parameter)
{
    struct state_machine m;
    int ch;

    statem_init(&m, &state_idle, &state_error);

    while (1)
    {
        if (RT_EOK == rt_mb_recv(mb_key, (rt_ubase_t *)&ch, 20))
        {
            statem_handle_event(&m, &(struct event){EVENT_KEYBOARD, (void *)(intptr_t)ch});
        }
    }
}

static int state_init(void)
{
    rt_thread_t tid = RT_NULL;

    mb_key = rt_mb_create("key", 8, RT_IPC_FLAG_FIFO);

    tid = rt_thread_create("state", state_process, RT_NULL,
                           1024, 10, 100);
    if (tid == RT_NULL)
    {
        rt_kprintf("state initialize failed! thread create failed!\r\n");

        return -RT_ENOMEM;
    }
    rt_thread_startup(tid);

    return RT_EOK;
}
INIT_APP_EXPORT(state_init);

#ifdef FINSH_USING_MSH
static void state_key_set(uint8_t argc, char **argv)
{
    if (argc == 2)
    {
        rt_kprintf("state key set:%c\n", argv[1][0]);
        rt_mb_send(mb_key, argv[1][0]);
    }
    else
    {
        rt_kprintf("state key set<a-z>\n");
    }
}
MSH_CMD_EXPORT(state_key_set, state key set<a - z>.);
#endif /* FINSH_USING_MSH */
