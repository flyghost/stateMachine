#include "state_machine.h"

static void go_to_state_error(struct state_machine *state_machine, struct event *const event);
static struct transition *get_transition(struct state_machine *state_machine, struct state *state, struct event *const event);

/**
 * @brief 初始化状态机
 * 
 * @param fsm           状态机
 * @param state_init    初始状态
 * @param state_error   错误状态
 * @return int          0：成功   -1：失败
 */
int statem_init(struct state_machine *fsm, struct state *state_init, struct state *state_error)
{
    if (!fsm)
    {
        return -1;
    }

    fsm->state_current = state_init;
    fsm->state_previous = NULL;
    fsm->state_error = state_error;

    return 0;
}

/**
 * @brief 状态机处理事件
 * 
 * 回调函数执行顺序：action_exti--->action--->action_entry
 *
 * @param fsm       状态机
 * @param event     事件
 * @return int
 */
int statem_handle_event(struct state_machine *fsm, struct event *event)
{
    if (!fsm || !event)
    {
        return STATEM_ERR_ARG;
    }

    if (!fsm->state_current)    // 当前状态为空，错误
    {
        go_to_state_error(fsm, event);
        return STATEM_ERR_STATE_RECHED;
    }

    /* 如果这个状态没有儿子，不用转移了,那他自己是儿子吗 */

    // 没有转换函数 且
    if ((!fsm->state_current->transition_nums) && (!fsm->state_current->state_parent))
    {
        return STATEM_STATE_NOCHANGE;
    }

    struct state *state_next = fsm->state_current; // 当前状态

    do
    {
        // 查找是否有满足条件的转换函数
        struct transition *transition = get_transition(fsm, state_next, event); // 根据当前状态，事件，得到转化函数（里面执行了gard函数，判断了转换条件）

        // 如果当前状态的给定事件没有转换，请检查是否有任何父状态的转换（如果有）：
        // 如果没有转移函数，则跳到上一个状态
        if (!transition)
        {
            state_next = state_next->state_parent;
            continue;
        }

        // 转移函数必须要有下一个状态，否则错误
        if (!transition->state_next)
        {
            go_to_state_error(fsm, event);
            return STATEM_ERR_STATE_RECHED;
        }

        // 存在转移函数，转到目标状态
        state_next = transition->state_next;

        // 如果新状态是父状态，则进入其入口状态（如果有的话）。 向下遍历整个家族树，直到找到没有入口状态的状态
        while (state_next->state_entry)
        {
            state_next = state_next->state_entry;
        }

        // 运行到这里，目标状态已经找到，开始执行退出函数

        // 离开上一个状态
        if (state_next != fsm->state_current && fsm->state_current->action_exti)        // 目标状态和当前状态不同， 且存在退出函数
        {
            fsm->state_current->action_exti(fsm->state_current->data, event);
        }

        // 执行转换函数
        if (transition->action)
        {
            transition->action(fsm->state_current->data, event, state_next->data);
        }

        // 保存上一个状态
        fsm->state_previous = fsm->state_current;

        // 执行新状态的入口函数
        if (state_next != fsm->state_current && state_next->action_entry)               // 目标状态和当前状态不同，且存在入口函数
        {
            state_next->action_entry(state_next->data, event);
        }

        // 更新状态
        fsm->state_current = state_next;

        // 当前转换是自身状态转换
        if (fsm->state_current == fsm->state_previous)
        {
            return STATEM_STATE_LOOPSELF;
        }

        // 当前状态是错误状态
        if (fsm->state_current == fsm->state_error)
        {
            return STATEM_ERR_STATE_RECHED;
        }

        // 当前状态没有转换函数，也没有父状态，状态机停止，无法进行下一次转换
        if ((!fsm->state_current->transition_nums) && (!fsm->state_current->state_parent))
        {
            return STATEM_FINAL_STATE_RECHED;
        }

        return STATEM_STATE_CHANGED;
    } while (state_next);

    return STATEM_STATE_NOCHANGE;
}

// 当前状态
struct state *statem_state_current(struct state_machine *fsm)
{
    if (!fsm)
    {
        return NULL;
    }

    return fsm->state_current;
}

// 上一个状态
struct state *statem_state_previous(struct state_machine *fsm)
{
    if (!fsm)
    {
        return NULL;
    }

    return fsm->state_previous;
}

// 进入错误状态
static void go_to_state_error(struct state_machine *fsm,
                              struct event *const event)
{
    fsm->state_previous = fsm->state_current;
    fsm->state_current = fsm->state_error;

    /* 本地错误状态要执行进入，进入错误状态肯定是数据设置错误，不是状态机不符合逻辑 */
    if (fsm->state_current && fsm->state_current->action_entry)
    {
        fsm->state_current->action_entry(fsm->state_current->data, event);
    }
}

// 状态、事件下对应着多个目标状态，需要根据条件判断走哪个状态
/**
 * @brief Get the transition object
 * 
 * @param fsm           状态机
 * @param state         
 * @param event 
 * @return struct transition* 
 */
static struct transition *get_transition(struct state_machine *fsm, struct state *state, struct event *const event)
{
    size_t i;

    if (!state)
    {
        return NULL;
    }

    // 循环所有的转换函数
    for (i = 0; i < state->transition_nums; ++i)
    {
        struct transition *t = &state->transitions[i];

        /* A transition for the given event has been found: */
        // 确保事件类型是相同的
        if (t->event_type == event->type)
        {
            // 事件类型相同就可以转换
            if (!t->guard)
            {
                return t;
            }
            // 条件是否满足，满足才可以转换
            else if (t->guard(t->condition, event))
            {
                return t;
            }
        }
    }

    /* No transitions found for given event for given state: */
    return NULL;
}

int statem_stopped(struct state_machine *state_machine)
{
    if (!state_machine)
    {
        return -1;
    }

    return (state_machine->state_current->transition_nums == 0);
}
