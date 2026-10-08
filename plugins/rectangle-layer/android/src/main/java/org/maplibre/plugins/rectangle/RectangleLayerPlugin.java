package org.maplibre.plugins.rectangle;

import org.maplibre.android.LibraryLoader;
import org.maplibre.android.MapLibre;

/** Registers the source-bound {@code rectangle} style layer. */
public final class RectangleLayerPlugin {
  public static final String ID = "org.maplibre.rectangle-layer";
  public static final String LAYER_TYPE = "rectangle";

  public enum RegistrationResult {
    REGISTERED,
    ALREADY_REGISTERED
  }

  private static boolean nativeLoaded;

  private RectangleLayerPlugin() {}

  /**
   * Registers the layer type with the loaded MapLibre renderer. Call after
   * {@link MapLibre#getInstance} and before loading a style that uses it.
   */
  public static synchronized RegistrationResult register() {
    // The renderer library exports mln_plugin_register_v1; make sure it is loaded.
    LibraryLoader.load();
    if (!nativeLoaded) {
      // The plugin retains native callbacks for the process lifetime; never unload it.
      System.loadLibrary("rectangle-layer");
      nativeLoaded = true;
    }
    NativeResult result = nativeRegister();
    if (result.status == 0) return RegistrationResult.REGISTERED;
    if (result.status == 1) return RegistrationResult.ALREADY_REGISTERED;
    throw new RectangleLayerRegistrationException(result.status, result.message);
  }

  private static native NativeResult nativeRegister();

  static final class NativeResult {
    final int status;
    final String message;

    NativeResult(int status, String message) {
      this.status = status;
      this.message = message;
    }
  }
}
