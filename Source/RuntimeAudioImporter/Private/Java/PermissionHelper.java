// Copied from Engine/Source/ThirdParty/AndroidPermission/permission_library/src/com/google/vr/sdk/samples/permission/PermissionHelper.java

package com.Plugins.RuntimeAudioImporter;

import android.app.Activity;
import android.util.Log;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;
import java.lang.reflect.Method;

public class PermissionHelper {
	private static final String LOG_TAG = "PermissionHelper";
	public static Activity getForegroundActivity()
	{
		Activity activity = null;
		//trying to find the activity PermissionFragment should be attached to
		//in the case of Daydream app, attach to GVRTransition2DActivity if there is any
		try {
			Class<?> clazz = Class.forName("com.google.vr.sdk.samples.transition.GVRTransition2DActivity");
			Method m = clazz.getMethod("getActivity", new Class[] {});
			activity = (Activity)m.invoke(null);
		} catch (Exception e) {
			Log.e(LOG_TAG, "GVRTransition2DActivity.getActivity() failed. Trying to get GameActivity.");
		}
		if (activity != null) return activity;
		//for non Daydream app, directly attach the fragment to GameActivity
		try {
			Class<?> clazz = Class.forName("com.epicgames.unreal.GameActivity");
			Method m = clazz.getMethod("Get", new Class[] {});
			return (Activity)m.invoke(null);
		} catch (Exception e) {
			Log.e(LOG_TAG, "GameActivity.Get() failed");
		}
		return null;
	}

	public static boolean checkPermission(String permission)
	{
		Activity activity = getForegroundActivity();
		if (activity==null) return false;
		if (ContextCompat.checkSelfPermission(activity, permission) ==
			PackageManager.PERMISSION_GRANTED) {
			Log.d(LOG_TAG, "checkPermission: " + permission + " has granted");
			return true;
		}
		else {
			Log.d(LOG_TAG, "checkPermission: " + permission + " has not granted");
			return false;
		}
	}

	public static void acquirePermissions(final String permissions[])
	{
		Activity activity = getForegroundActivity();
		PermissionHelper.acquirePermissions(permissions, activity);
	}

	public static void acquirePermissions(final String permissions[], Activity InActivity)
	{
		if (InActivity==null) return;
		final Activity activity = InActivity;
		activity.runOnUiThread(new Runnable(){
			public void run() {
				PermissionFragment fragment = PermissionFragment.getInstance(activity);
				if (fragment!=null) {
					fragment.acquirePermissions(permissions);
				}
			}
		});
	}

	public static native void onAcquirePermissions(String permissions[], int grantResults[]);
}
