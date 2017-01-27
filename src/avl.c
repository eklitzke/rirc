#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "avl.h"
#include "utils.h"

#define H(N) (N == NULL ? 0 : N->height)
#define MAX(A, B) (A > B ? A : B)

//FIXME:
static jmp_buf jmpbuf;

static avl_node* _avl_add(avl_node*, const char*, void*);
static avl_node* _avl_del(avl_node*, const char*);
static avl_node* _avl_get(avl_node*, const char*, size_t);
static avl_node* avl_new_node(const char*, void*);
static void avl_free_node(avl_node*);
static avl_node* avl_rotate_L(avl_node*);
static avl_node* avl_rotate_R(avl_node*);

/* AVL tree functions */

void
free_avl(avl_node *n)
{
	/* Recusrively free an AVL tree */

	if (n == NULL)
		return;

	free_avl(n->l);
	free_avl(n->r);
	avl_free_node(n);
}

int
avl_add(avl_node **n, const char *key, void *val)
{
	/* Entry point for adding a node to an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_add(*n, key, val);

	return 1;
}

int
avl_del(avl_node **n, const char *key)
{
	/* Entry point for removing a node from an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_del(*n, key);

	return 1;
}

const avl_node*
avl_get(avl_node *n, const char *key, size_t len)
{
	/* Entry point for fetching an avl node with prefix key */

	if (setjmp(jmpbuf))
		return NULL;

	return _avl_get(n, key, len);
}

static avl_node*
avl_new_node(const char *key, void *val)
{
	avl_node *n;

	if ((n = calloc(1, sizeof(*n))) == NULL)
		fatal("calloc");

	n->height = 1;
	n->key = strdup(key);
	n->val = val;

	return n;
}

static void
avl_free_node(avl_node *n)
{
	free(n->key);
	free(n->val);
	free(n);
}

static avl_node*
avl_rotate_R(avl_node *r)
{
	/* Rotate right for root r and pivot p
	 *
	 *     r          p
	 *    / \   ->   / \
	 *   p   c      a   r
	 *  / \            / \
	 * a   b          b   c
	 *
	 */

	avl_node *p = r->l;
	avl_node *b = p->r;

	p->r = r;
	r->l = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
avl_rotate_L(avl_node *r)
{
	/* Rotate left for root r and pivot p
	 *
	 *   r            p
	 *  / \    ->    / \
	 * a   p        r   c
	 *    / \      / \
	 *   b   c    a   b
	 *
	 */

	avl_node *p = r->r;
	avl_node *b = p->l;

	p->l = r;
	r->r = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
_avl_add(avl_node *n, const char *key, void *val)
{
	/* Recursively add key to an AVL tree.
	 *
	 * If a duplicate is found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		return avl_new_node(key, val);

	int ret = strcasecmp(key, n->key);

	if (ret == 0)
		/* Duplicate found */
		longjmp(jmpbuf, 1);

	else if (ret > 0)
		n->r = _avl_add(n->r, key, val);

	else if (ret < 0)
		n->l = _avl_add(n->l, key, val);

	/* Node was successfully added, recaculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (strcasecmp(key, n->l->key) > 0)
			n->l = avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (strcasecmp(n->r->key, key) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_del(avl_node *n, const char *key)
{
	/* Recursive function for deleting nodes from an AVL tree
	 *
	 * If the node isn't found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		/* Node not found */
		longjmp(jmpbuf, 1);

	int ret = strcasecmp(key, n->key);

	if (ret == 0) {
		/* Node found */

		if (n->l && n->r) {
			/* Recursively delete nodes with both children to ensure balance */

			/* Find the next largest value in the tree (the leftmost node in the right subtree) */
			avl_node *next = n->r;

			while (next->l)
				next = next->l;

			/* Swap it's value with the node being deleted */
			avl_node t = *n;

			n->key = next->key;
			n->val = next->val;
			next->key = t.key;
			next->val = t.val;

			/* Recusively delete in the right subtree */
			n->r = _avl_del(n->r, t.key);

		} else {
			/* If n has a child, return it */
			avl_node *tmp = (n->l) ? n->l : n->r;

			avl_free_node(n);

			return tmp;
		}
	}

	else if (ret > 0)
		n->r = _avl_del(n->r, key);

	else if (ret < 0)
		n->l = _avl_del(n->l, key);

	/* Node was successfully deleted, recalculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (H(n->l->l) - H(n->l->r) < 0)
			n->l =  avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (H(n->r->l) - H(n->r->r) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_get(avl_node *n, const char *key, size_t len)
{
	/* Case insensitive search for a node whose value is prefixed by key */

	/* Failed to find node */
	if (n == NULL)
		longjmp(jmpbuf, 1);

	int ret = strncasecmp(key, n->key, len);

	if (ret > 0)
		return _avl_get(n->r, key, len);

	if (ret < 0)
		return _avl_get(n->l, key, len);

	/* Match found */
	return n;
}
