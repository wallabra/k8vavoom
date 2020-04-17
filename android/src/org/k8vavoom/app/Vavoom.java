package org.k8vavoom.app;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

public class Vavoom extends SDLActivity {
  @Override
  protected String[] getLibraries() {
    return new String[] {
      "crystax",
      "gnustl_shared",
      "openal",
      "GL",
      "SDL2",
      "k8vavoom"
    };
  }

  @Override
  protected String[] getArguments() {
    return new String[0];
  }

}
