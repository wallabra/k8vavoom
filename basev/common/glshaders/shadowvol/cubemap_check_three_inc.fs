
      // center
      uc = 2.0*(texX+0.5)/CubeSize-1.0;
      vc = 2.0*(texY+0.5)/CubeSize-1.0;
      newCubeDir = SMCHECK_V3;
      dv = dot(Normal, newCubeDir);
      //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
      t = ssd/dv;
      newCubeDir *= t;
      newCubeDist = dot(newCubeDir, newCubeDir);
      if (sldist >= newCubeDist) return 1.0;

      // corner #1
      uc = 2.0*(texX+0.01)/CubeSize-1.0;
      vc = 2.0*(texY+0.01)/CubeSize-1.0;
      vc1 = vc;
      newCubeDir = SMCHECK_V3;
      dv = dot(Normal, newCubeDir);
      //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
      t = ssd/dv;
      newCubeDir *= t;
      newCubeDist = dot(newCubeDir, newCubeDir);
      if (sldist >= newCubeDist) return 1.0;

      // corner #2
      uc = 2.0*(texX+0.99)/CubeSize-1.0;
      vc = 2.0*(texY+0.99)/CubeSize-1.0;
      newCubeDir = SMCHECK_V3;
      dv = dot(Normal, newCubeDir);
      //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
      t = ssd/dv;
      newCubeDir *= t;
      newCubeDist = dot(newCubeDir, newCubeDir);
      if (sldist >= newCubeDist) return 1.0;

      return 0.0;
