// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentDevice *device;
  GObject      *extension;
} DevicePluginFixture;

static void
device_fixture_set_up (DevicePluginFixture *fixture,
                       gconstpointer        user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");

  fixture->device = g_object_new (VALENT_TYPE_DEVICE,
                                  "id", "mock-device",
                                  NULL);
  fixture->extension = peas_engine_create_extension (engine,
                                                     plugin_info,
                                                     VALENT_TYPE_DEVICE_PLUGIN,
                                                     "object", fixture->device,
                                                     NULL);
}

static void
device_fixture_tear_down (DevicePluginFixture *fixture,
                          gconstpointer  user_data)
{
  v_await_finalize_object (fixture->device);
  v_await_finalize_object (fixture->extension);
}

static void
test_device_plugin_basic (DevicePluginFixture *fixture,
                          gconstpointer        user_data)
{
  g_autoptr (ValentDevice) device = NULL;
  PeasPluginInfo *plugin_info = NULL;

  /* Test properties */
  g_object_get (fixture->extension,
                "object",      &device,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_true (VALENT_IS_DEVICE (device));
  g_assert_nonnull (plugin_info);

  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);
}

static void
on_activate (GSimpleAction *action,
             GVariant      *parameter,
             gboolean      *activated)
{
  if (activated)
    *activated = g_variant_get_boolean (parameter);
}

static void
on_action_signal (gboolean *emitted)
{
  if (emitted != NULL)
    *emitted = TRUE;
}

static void
test_device_plugin_actions (DevicePluginFixture *fixture,
                            gconstpointer        user_data)
{
  g_autoptr (GSimpleAction) action = NULL;
  gboolean has_action = FALSE;
  gboolean enabled = FALSE;
  const GVariantType *parameter_type = NULL;
  const GVariantType *state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *state = NULL;
  gboolean emitted = FALSE;

  VALENT_TEST_CHECK ("Actions can be queried");
  has_action = g_action_group_query_action (G_ACTION_GROUP (fixture->extension),
                                            "state",
                                            &enabled,
                                            &parameter_type,
                                            &state_type,
                                            &state_hint,
                                            &state);
  g_assert_true (has_action);
  g_assert_true (enabled);
  g_assert_null (parameter_type);
  g_assert_true (g_variant_type_equal (state_type, G_VARIANT_TYPE_BOOLEAN));
  g_assert_null (state_hint);
  g_assert_true (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  /* Signals */
  g_object_connect (G_OBJECT (fixture->extension),
                    "swapped-signal::action-added",           on_action_signal, &emitted,
                    "swapped-signal::action-enabled-changed", on_action_signal, &emitted,
                    "swapped-signal::action-removed",         on_action_signal, &emitted,
                    "swapped-signal::action-state-changed",   on_action_signal, &emitted,
                    NULL);

  VALENT_TEST_CHECK ("Stateful actions can be changed");
  g_action_group_change_action_state (G_ACTION_GROUP (fixture->extension),
                                      "state",
                                      g_variant_new_boolean (FALSE));
  valent_test_await_boolean (&emitted);

  VALENT_TEST_CHECK ("Stateful actions can be read");
  state = g_action_group_get_action_state (G_ACTION_GROUP (fixture->extension),
                                           "state");
  g_assert_false (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Actions can be added");
  action = g_simple_action_new ("action", G_VARIANT_TYPE_BOOLEAN);
  g_action_map_add_action (G_ACTION_MAP (fixture->extension),
                           G_ACTION (action));
  valent_test_await_boolean (&emitted);

  VALENT_TEST_CHECK ("Actions can be disabled");
  g_simple_action_set_enabled (action, FALSE);
  valent_test_await_boolean (&emitted);

  VALENT_TEST_CHECK ("Actions can be enabled");
  g_simple_action_set_enabled (action, TRUE);
  valent_test_await_boolean (&emitted);

  VALENT_TEST_CHECK ("Actions can be activated");
  g_signal_connect (action,
                    "activate",
                    G_CALLBACK (on_activate),
                    &emitted);
  g_action_group_activate_action (G_ACTION_GROUP (fixture->extension),
                                  "action",
                                  g_variant_new_boolean (TRUE));
  valent_test_await_boolean (&emitted);

  VALENT_TEST_CHECK ("Actions can be removed");
  g_action_map_remove_action (G_ACTION_MAP (fixture->extension), "action");
  valent_test_await_boolean (&emitted);

  g_signal_handlers_disconnect_by_data (fixture->extension, &emitted);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/device/device-plugin/basic",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_basic,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device-plugin/actions",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_actions,
              device_fixture_tear_down);

  return g_test_run ();
}

