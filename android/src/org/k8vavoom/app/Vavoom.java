package org.k8vavoom.app;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import android.content.Intent;

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
    Intent intent = getIntent();
    String value = intent.getStringExtra("CommandLine");
    String[] args = value.split("[ \n\t]+");
    return args;
  }

}
