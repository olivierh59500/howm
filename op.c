#include "op.h"
#include "howm.h"
#include "workspace.h"
#include "client.h"

/**
 * @brief An operator that kills an arbitrary amount of clients or workspaces.
 *
 * @param type Whether to kill workspaces or clients.
 * @param cnt How many workspaces or clients to kill.
 */
void op_kill(const unsigned int type, int cnt)
{
	if (type == WORKSPACE) {
		log_info("Killing %d workspaces", cnt);
		while (cnt > 0) {
			kill_ws(correct_ws(cw + cnt - 1));
			cnt--;
		}
	} else if (type == CLIENT) {
		log_info("Killing %d clients", cnt);
		while (cnt > 0) {
			kill_client(cw, cnt == 1);
			cnt--;
		}
	}
}

/**
 * @brief Move client/s down.
 *
 * @param type We don't support moving workspaces, so this should only be
 * client.
 * @param cnt How many clients to move.
 */
void op_move_down(const unsigned int type, int cnt)
{
	if (type == WORKSPACE)
		return;
	move_client(cnt, false);
}

/**
 * @brief Move client/s up.
 *
 * @param type We don't support moving workspaces, so this should only be
 * client.
 * @param cnt How many clients to move.
 */
void op_move_up(const unsigned int type, int cnt)
{
	if (type == WORKSPACE)
		return;
	move_client(cnt, true);
}

/**
 * @brief Operator function to move the current focus up.
 *
 * @param type Whether to focus on clients or workspaces.
 * @param cnt The number of times to move focus.
 */
void op_focus_up(const unsigned int type, int cnt)
{
	while (cnt > 0) {
		if (type == CLIENT)
			focus_next_client(NULL);
		else if (type == WORKSPACE)
			focus_next_ws(NULL);
		else
			return;
		cnt--;
	}
}

/**
 * @brief Operator function to move the current focus down.
 *
 * @param type Whether to focus on clients or workspaces.
 * @param cnt The number of times to move focus.
 */
void op_focus_down(const unsigned int type, int cnt)
{
	while (cnt > 0) {
		if (type == CLIENT)
			focus_prev_client(NULL);
		else if (type == WORKSPACE)
			focus_prev_ws(NULL);
		else
			return;
		cnt--;
	}
}

/**
 * @brief An operator to grow the gaps of either workspaces or clients by
 * OP_GAP_SIZE.
 *
 * When the type is workspace, the gap size for that workspace is also changed.
 * This means that new windows will be spawned in with the modified gap size.
 *
 * @param type Whether the operation should be performed on a client or
 * workspace.
 * @param cnt The amount of clients or workspaces to perform the operation on.
 */
void op_grow_gaps(const unsigned int type, int cnt)
{
	change_gaps(type, cnt, OP_GAP_SIZE);
}

/**
 * @brief Does the heavy lifting of changing the gaps of clients.
 *
 * @param type Whether to perform the operation on a client or workspace.
 * @param cnt The amount of times to perform the operation.
 * @param size The amount of pixels to change the gap size by. This is
 * configured through OP_GAP_SIZE.
 */
void change_gaps(const unsigned int type, int cnt, int size)
{
	Client *c = NULL;

	if (type == WORKSPACE) {
		while (cnt > 0) {
			cnt--;
			wss[correct_ws(cw + cnt)].gap += size;
			log_info("Changing gaps of workspace <%d> by %dpx",
					correct_ws(cw + cnt), size);
			for (c = wss[correct_ws(cw + cnt)].head; c; c = c->next)
				change_client_gaps(c, size);
		}
	} else if (type == CLIENT) {
		c = wss[cw].current;
		while (cnt > 0) {
			log_info("Changing gaps of client <%p> by %dpx", c, size);
			change_client_gaps(c, size);
			c = next_client(c);
			cnt--;
		}
	}
}

/**
 * @brief Cut one or more clients and add them onto howm's delete register
 * stack (if there is space).
 *
 * A segment of howm's internal client list is taken and placed onto the delete
 * register stack. All clients from the list segment must be unmapped and the
 * remaining clients must be refocused.
 *
 * @param type Whether to cut an entire workspace or client.
 * @param cnt The amount of clients or workspaces to cut.
 */
void op_cut(const unsigned int type, int cnt)
{
	Client *tail = wss[cw].current;
	Client *head = wss[cw].current;
	Client *head_prev = prev_client(wss[cw].current, cw);
	bool wrap = false;

	if (!head)
		return;

	if (del_reg.size >= DELETE_REGISTER_SIZE) {
		log_warn("No more stack space.");
		return;
	}

	if ((type == CLIENT && cnt >= wss[cw].client_cnt) || type == WORKSPACE) {
		if (cnt + del_reg.size > DELETE_REGISTER_SIZE)
			return;

		while (cnt > 0) {
			head = wss[correct_ws(cw + cnt - 1)].head;
			for (tail = head; tail; tail = tail->next)
				xcb_unmap_window(dpy, tail->win);
			stack_push(&del_reg, head);
			wss[correct_ws(cw + cnt - 1)].head = NULL;
			wss[correct_ws(cw + cnt - 1)].prev_foc = NULL;
			wss[correct_ws(cw + cnt - 1)].current = NULL;
			cnt--;
			wss[correct_ws(cw + cnt - 1)].client_cnt = 0;
		}

	} else if (type == CLIENT) {
		xcb_unmap_window(dpy, head->win);
		wss[cw].client_cnt--;
		while (cnt > 1) {
			if (!tail->next && next_client(tail)) {
				wrap = true;
				/* Join the list into a circular linked list,
				 * just for now so that we don't miss any
				 * clients. */
				tail->next = next_client(tail);
			}
			if (tail == wss[cw].prev_foc)
				wss[cw].prev_foc = NULL;
			tail = next_client(tail);
			xcb_unmap_window(dpy, tail->win);
			cnt--;
			wss[cw].client_cnt--;
		}

		if (head == wss[cw].head) {
			wss[cw].head = head == next_client(tail) ? NULL : next_client(tail);
		} else if (wrap) {
			wss[cw].head = tail->next;
			head_prev->next = NULL;
		} else if (tail->next != head_prev) {
			head_prev->next = wrap ? NULL : tail->next;
		}

		wss[cw].current = head_prev;
		tail->next = NULL;
		update_focused_client(head_prev);
		stack_push(&del_reg, head);
	}
}
