/*
 * tests_tree_multi_selection.cpp
 *
 * Copyright 2009-2026
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "tests_multi_node.h"

#include "ct_actions.h"
#include "ct_main_win.h"
#include "tests_common.h"
#include <stdexcept>

namespace {

void process_gtk_events()
{
#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
    while (gtk_events_pending()) gtk_main_iteration();
#else
    while (g_main_context_pending(nullptr)) g_main_context_iteration(nullptr, false);
#endif
}

void respond_to_next_dialog(const int response)
{
    Glib::signal_idle().connect_once([response](){
#if GTKMM_MAJOR_VERSION >= 4
        GListModel* windows = gtk_window_get_toplevels();
        const guint count = g_list_model_get_n_items(windows);
        for (guint i = 0; i < count; ++i) {
            auto* window = GTK_WINDOW(g_list_model_get_item(windows, i));
            if (GTK_IS_DIALOG(window)) gtk_dialog_response(GTK_DIALOG(window), response);
            g_object_unref(window);
        }
#else
        GList* windows = gtk_window_list_toplevels();
        for (GList* item = windows; item; item = item->next) {
            if (GTK_IS_DIALOG(item->data)) gtk_dialog_response(GTK_DIALOG(item->data), response);
        }
        g_list_free(windows);
#endif
    });
}

#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
Gtk::MenuItem* find_menu_item_by_action(Gtk::MenuShell* menu, const std::string& action_id)
{
    for (Gtk::Widget* child : menu->get_children()) {
        auto* item = dynamic_cast<Gtk::MenuItem*>(child);
        if (not item) continue;
        if (item->get_child() and item->get_child()->get_name() == action_id) return item;
        if (Gtk::Menu* submenu = item->get_submenu()) {
            if (Gtk::MenuItem* found = find_menu_item_by_action(submenu, action_id)) return found;
        }
    }
    return nullptr;
}
#endif

Gtk::TreeModel::iterator append_test_node(CtMainWin* pWin,
                                          const char* name,
                                          Gtk::TreeModel::iterator* parent = nullptr)
{
    CtNodeData node_data{};
    node_data.nodeId = pWin->get_tree_store().node_id_get();
    node_data.name = name;
    node_data.syntax = CtConst::PLAIN_TEXT_ID;
    node_data.pTextBuffer = pWin->get_new_text_buffer("temporary\n");
    return pWin->get_tree_store().append_node(&node_data, parent);
}

void select_primary_nodes(CtMainWin* pWin,
                          const Gtk::TreeModel::iterator& first,
                          const Gtk::TreeModel::iterator& second)
{
    auto selection = pWin->get_tree_view().get_selection();
    selection->unselect_all();
    pWin->get_tree_view().set_cursor_safe(second);
    selection->select(pWin->get_tree_store().get_path(first));
    process_gtk_events();
}

}

void assert_tree_multi_selection_actions(CtMainWin* pWin)
{
    auto first = pWin->get_tree_store().get_iter_first();
    ASSERT_TRUE(first);
    auto second = first;
    ++second;
    ASSERT_TRUE(second);
    select_primary_nodes(pWin, first, second);

    CtMenu& menu = pWin->get_ct_menu();
    const CtTreeSelectionSnapshot snapshot = pWin->tree_selection_snapshot();
    ASSERT_EQ(2u, snapshot.physicalRows.size());

    size_t tree_action_count{0};
    size_t single_action_count{0};
    size_t batch_action_count{0};
    size_t navigation_action_count{0};
    size_t independent_action_count{0};
    size_t preserve_order_action_count{0};
    for (const CtMenuAction& action : menu.get_actions()) {
        if (action.category != _("Tree")) continue;
        ++tree_action_count;
        EXPECT_NE(CtTreeSelectionSemantics::NotTreeAction, action.treeSelectionSemantics) << action.id;
        switch (action.treeSelectionSemantics) {
            case CtTreeSelectionSemantics::SinglePhysicalRow: ++single_action_count; break;
            case CtTreeSelectionSemantics::BatchPhysicalRows: ++batch_action_count; break;
            case CtTreeSelectionSemantics::NavigationToSingleRow: ++navigation_action_count; break;
            case CtTreeSelectionSemantics::SelectionIndependent: ++independent_action_count; break;
            case CtTreeSelectionSemantics::PreserveSelectionAndRefreshOrder: ++preserve_order_action_count; break;
            case CtTreeSelectionSemantics::NotTreeAction: break;
        }
        const bool expected_enabled = action.treeSelectionSemantics != CtTreeSelectionSemantics::SinglePhysicalRow;
        EXPECT_EQ(expected_enabled, menu.is_action_enabled(action, snapshot)) << action.id;
    }
    EXPECT_EQ(32u, tree_action_count);
    EXPECT_EQ(18u, single_action_count);
    EXPECT_EQ(5u, batch_action_count);
    EXPECT_EQ(4u, navigation_action_count);
    EXPECT_EQ(3u, independent_action_count);
    EXPECT_EQ(2u, preserve_order_action_count);

    size_t rows_before{0};
    pWin->get_tree_store().get_store()->foreach_iter([&](const Gtk::TreeModel::iterator&){ ++rows_before; return false; });
    menu.activate_action("tree_dup_node");
    size_t rows_after{0};
    pWin->get_tree_store().get_store()->foreach_iter([&](const Gtk::TreeModel::iterator&){ ++rows_after; return false; });
    EXPECT_EQ(rows_before, rows_after);

    auto window_action = pWin->lookup_action("tree_add_node");
    ASSERT_TRUE(window_action);
    EXPECT_FALSE(window_action->get_enabled());
#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
    auto* popup = menu.get_popup_menu(CtMenu::POPUP_MENU_TYPE::Node);
    auto* add_item = find_menu_item_by_action(popup, "tree_add_node");
    ASSERT_NE(nullptr, add_item);
    EXPECT_FALSE(add_item->get_sensitive());
#endif

    CtTreeIter first_iter = pWin->get_tree_store().to_ct_tree_iter(first);
    CtTreeIter second_iter = pWin->get_tree_store().to_ct_tree_iter(second);
    const bool first_read_only = first_iter.get_node_read_only();
    const bool second_read_only = second_iter.get_node_read_only();
    first_iter.set_node_read_only(false);
    second_iter.set_node_read_only(true);
    menu.activate_action("tree_node_toggle_ro");
    EXPECT_TRUE(first_iter.get_node_read_only());
    EXPECT_TRUE(second_iter.get_node_read_only());
    menu.activate_action("tree_node_toggle_ro");
    EXPECT_FALSE(first_iter.get_node_read_only());
    EXPECT_FALSE(second_iter.get_node_read_only());
    first_iter.set_node_read_only(first_read_only);
    second_iter.set_node_read_only(second_read_only);

    const bool first_bookmarked = pWin->get_tree_store().is_node_bookmarked(first_iter.get_node_id());
    const bool second_bookmarked = pWin->get_tree_store().is_node_bookmarked(second_iter.get_node_id());
    menu.activate_action("node_bookmark");
    EXPECT_TRUE(pWin->get_tree_store().is_node_bookmarked(first_iter.get_node_id()));
    EXPECT_TRUE(pWin->get_tree_store().is_node_bookmarked(second_iter.get_node_id()));
    menu.activate_action("node_unbookmark");
    EXPECT_FALSE(pWin->get_tree_store().is_node_bookmarked(first_iter.get_node_id()));
    EXPECT_FALSE(pWin->get_tree_store().is_node_bookmarked(second_iter.get_node_id()));
    if (first_bookmarked) pWin->get_tree_store().bookmarks_add(first_iter.get_node_id());
    if (second_bookmarked) pWin->get_tree_store().bookmarks_add(second_iter.get_node_id());
    pWin->menu_set_bookmark_menu_items();

#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
    menu.activate_action("tree_node_link");
    const std::string copied_links = Gtk::Clipboard::get()->wait_for_text().raw();
    EXPECT_NE(std::string::npos, copied_links.find(first_iter.get_node_name().raw()));
    EXPECT_NE(std::string::npos, copied_links.find(second_iter.get_node_name().raw()));
    EXPECT_NE(std::string::npos, copied_links.find("\n"));
#endif
}

void assert_tree_batch_delete(CtMainWin* pWin)
{
    auto root_a = append_test_node(pWin, "multi-delete-a");
    auto child_a = append_test_node(pWin, "multi-delete-a-child", &root_a);
    auto root_b = append_test_node(pWin, "multi-delete-b");
    auto survivor = append_test_node(pWin, "multi-delete-survivor");
    const gint64 root_a_id = pWin->get_tree_store().to_ct_tree_iter(root_a).get_node_id();
    const gint64 child_a_id = pWin->get_tree_store().to_ct_tree_iter(child_a).get_node_id();
    const gint64 root_b_id = pWin->get_tree_store().to_ct_tree_iter(root_b).get_node_id();
    const gint64 survivor_id = pWin->get_tree_store().to_ct_tree_iter(survivor).get_node_id();

    auto selection = pWin->get_tree_view().get_selection();
    selection->unselect_all();
    pWin->get_tree_view().expand_row(pWin->get_tree_store().get_path(root_a), false);
    pWin->get_tree_view().set_cursor_safe(root_b);
    selection->select(pWin->get_tree_store().get_path(root_a));
    selection->select(pWin->get_tree_store().get_path(child_a));
    process_gtk_events();
    ASSERT_EQ(3u, pWin->tree_selection_snapshot().physicalRows.size());

    pWin->get_tree_store().to_ct_tree_iter(root_b).set_node_read_only(true);
    CtTreeDeletePlan plan = pWin->get_ct_actions()->build_tree_delete_plan(pWin->tree_selection_snapshot());
    ASSERT_EQ(2u, plan.rootNodeIds.size());
    EXPECT_EQ(3u, plan.removalNodeIds.size());
    EXPECT_TRUE(plan.removalNodeIdSet.count(child_a_id));
    EXPECT_EQ(survivor_id, plan.fallbackNodeId);
    EXPECT_TRUE(plan.hasReadOnlyRoot);

    respond_to_next_dialog(GTK_RESPONSE_OK);
    pWin->get_ct_menu().activate_action("tree_node_del");
    EXPECT_TRUE(pWin->get_tree_store().get_node_from_node_id(root_a_id));
    EXPECT_TRUE(pWin->get_tree_store().get_node_from_node_id(root_b_id));

    pWin->get_tree_store().to_ct_tree_iter(root_b).set_node_read_only(false);
    pWin->get_tree_store().bookmarks_add(child_a_id);
    pWin->get_tree_store().bookmarks_add(root_b_id);
    plan = pWin->get_ct_actions()->build_tree_delete_plan(pWin->tree_selection_snapshot());
    EXPECT_FALSE(plan.hasReadOnlyRoot);
    respond_to_next_dialog(GTK_RESPONSE_YES);
    pWin->get_ct_menu().activate_action("tree_node_del");
    process_gtk_events();

    EXPECT_FALSE(pWin->get_tree_store().get_node_from_node_id(root_a_id));
    EXPECT_FALSE(pWin->get_tree_store().get_node_from_node_id(child_a_id));
    EXPECT_FALSE(pWin->get_tree_store().get_node_from_node_id(root_b_id));
    EXPECT_FALSE(pWin->get_tree_store().is_node_bookmarked(child_a_id));
    EXPECT_FALSE(pWin->get_tree_store().is_node_bookmarked(root_b_id));
    EXPECT_TRUE(pWin->get_tree_store().get_node_from_node_id(survivor_id));

    if (CtTreeIter survivor_iter = pWin->get_tree_store().get_node_from_node_id(survivor_id)) {
        auto batch_guard = pWin->tree_model_batch_guard(CtMainWin::TreeModelBatchMode::DetachEditorBindings);
        pWin->get_tree_store().get_store()->erase(survivor_iter);
    }

    auto master = append_test_node(pWin, "multi-delete-shared-master");
    CtTreeIter master_iter = pWin->get_tree_store().to_ct_tree_iter(master);
    CtNodeData shared_data{};
    pWin->get_tree_store().get_node_data(master_iter, shared_data, true);
    shared_data.nodeId = pWin->get_tree_store().node_id_get();
    shared_data.sharedNodesMasterId = master_iter.get_node_id();
    auto alias_removed = pWin->get_tree_store().append_node(&shared_data);
    shared_data.nodeId = pWin->get_tree_store().node_id_get();
    auto alias_survives = pWin->get_tree_store().append_node(&shared_data);
    const gint64 master_id = master_iter.get_node_id();
    const gint64 alias_removed_id = pWin->get_tree_store().to_ct_tree_iter(alias_removed).get_node_id();
    const gint64 alias_survives_id = pWin->get_tree_store().to_ct_tree_iter(alias_survives).get_node_id();

    selection->unselect_all();
    pWin->get_tree_view().set_cursor_safe(alias_removed);
    selection->select(pWin->get_tree_store().get_path(master));
    process_gtk_events();
    ASSERT_EQ(2u, pWin->tree_selection_snapshot().physicalRows.size());
    ASSERT_EQ(1u, pWin->tree_selection_snapshot().logicalDataHolders.size());
    plan = pWin->get_ct_actions()->build_tree_delete_plan(pWin->tree_selection_snapshot());
    EXPECT_EQ(2u, plan.rootNodeIds.size());
    EXPECT_EQ(alias_survives_id, plan.fallbackNodeId);
    respond_to_next_dialog(GTK_RESPONSE_YES);
    pWin->get_ct_menu().activate_action("tree_node_del");
    process_gtk_events();

    EXPECT_FALSE(pWin->get_tree_store().get_node_from_node_id(master_id));
    EXPECT_FALSE(pWin->get_tree_store().get_node_from_node_id(alias_removed_id));
    CtTreeIter promoted_master = pWin->get_tree_store().get_node_from_node_id(alias_survives_id);
    ASSERT_TRUE(promoted_master);
    EXPECT_EQ(0, promoted_master.get_node_shared_master_id());
    {
        auto batch_guard = pWin->tree_model_batch_guard(CtMainWin::TreeModelBatchMode::DetachEditorBindings);
        pWin->get_tree_store().get_store()->erase(promoted_master);
    }
}

void assert_empty_tree_batch_delete(CtMainWin* pWin)
{
    static bool tested{false};
    if (tested) return;
    tested = true;

    auto root_a = append_test_node(pWin, "z-delete-all");
    auto child_a = append_test_node(pWin, "delete-all-a-child", &root_a);
    auto root_b = append_test_node(pWin, "a-delete-all");
    CtTreeIter root_a_iter = pWin->get_tree_store().to_ct_tree_iter(root_a);
    CtTreeIter root_b_iter = pWin->get_tree_store().to_ct_tree_iter(root_b);
    const gint64 child_a_id = pWin->get_tree_store().to_ct_tree_iter(child_a).get_node_id();

    pWin->select_tree_iter_only(root_a_iter);
    process_gtk_events();
    ASSERT_EQ(root_a_iter.get_node_id(), pWin->curr_tree_iter().get_node_id());
    {
        auto outer_guard = pWin->tree_model_batch_guard();
        {
            auto inner_guard = pWin->tree_model_batch_guard();
            pWin->select_tree_iter_only(root_b_iter);
        }
        EXPECT_EQ(root_a_iter.get_node_id(), pWin->curr_tree_iter().get_node_id());
    }

    try {
        auto batch_guard = pWin->tree_model_batch_guard();
        throw std::runtime_error{"test guard unwinding"};
    }
    catch (const std::runtime_error&) {
    }
    pWin->select_tree_iter_only(root_b_iter);
    process_gtk_events();
    ASSERT_EQ(root_b_iter.get_node_id(), pWin->curr_tree_iter().get_node_id());

    auto selection = pWin->get_tree_view().get_selection();
    selection->unselect_all();
    pWin->get_tree_view().expand_row(pWin->get_tree_store().get_path(root_a), false);
    pWin->get_tree_view().set_cursor_safe(root_b);
    selection->select(pWin->get_tree_store().get_path(root_a));
    selection->select(pWin->get_tree_store().get_path(child_a));
    process_gtk_events();

    pWin->get_ct_menu().activate_action("tree_all_sort_asc");
    process_gtk_events();
    const CtTreeSelectionSnapshot sorted_selection = pWin->tree_selection_snapshot();
    ASSERT_EQ(3u, sorted_selection.physicalRows.size());
    EXPECT_EQ(root_b_iter.get_node_id(), sorted_selection.physicalRows[0].get_node_id());
    EXPECT_EQ(root_a_iter.get_node_id(), sorted_selection.physicalRows[1].get_node_id());
    EXPECT_EQ(child_a_id, sorted_selection.physicalRows[2].get_node_id());

    const CtTreeDeletePlan plan = pWin->get_ct_actions()->build_tree_delete_plan(pWin->tree_selection_snapshot());
    ASSERT_EQ(2u, plan.rootNodeIds.size());
    EXPECT_EQ(3u, plan.removalNodeIds.size());
    EXPECT_EQ(-1, plan.fallbackNodeId);
    respond_to_next_dialog(GTK_RESPONSE_YES);
    pWin->get_ct_menu().activate_action("tree_node_del");
    process_gtk_events();

    EXPECT_FALSE(pWin->get_tree_store().get_iter_first());
    EXPECT_TRUE(pWin->tree_selection_snapshot().physicalRows.empty());
    EXPECT_FALSE(pWin->tree_cursor_iter());
    EXPECT_FALSE(pWin->curr_tree_iter());
    ASSERT_TRUE(pWin->get_text_view().get_buffer());
    EXPECT_EQ(0, pWin->get_text_view().get_buffer()->get_char_count());
    EXPECT_FALSE(pWin->get_text_view().mm().get_sensitive());
    pWin->update_window_save_not_needed();
}

void queue_deferred_menu_action_for_teardown(CtMainWin* pWin)
{
#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
    auto* popup = pWin->get_ct_menu().get_popup_menu(CtMenu::POPUP_MENU_TYPE::Node);
    Gtk::MenuItem* item = find_menu_item_by_action(popup, "nodes_all_expand");
    ASSERT_NE(nullptr, item);
    item->activate();
#else
    (void)pWin;
#endif
}
