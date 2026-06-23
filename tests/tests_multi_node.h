/*
 * tests_multi_node.h
 *
 * Copyright 2009-2026
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 */

#pragma once

class CtMainWin;

void assert_multi_node_selection(CtMainWin* pWin);
void assert_tree_multi_selection_actions(CtMainWin* pWin);
void assert_tree_batch_delete(CtMainWin* pWin);
void assert_empty_tree_batch_delete(CtMainWin* pWin);
void queue_deferred_menu_action_for_teardown(CtMainWin* pWin);
