// Copied from Engine/Source/ThirdParty/AndroidPermission/permission_library/src/com/google/vr/sdk/samples/permission/PermissionFragment.java

package com.Plugins.RuntimeAudioImporter;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Build;
import androidx.legacy.app.FragmentCompat;
import android.util.Log;

public class PermissionFragment extends Fragment
		implements FragmentCompat.OnRequestPermissionsResultCallback {

	private static final int PERMISSION_REQUEST_CODE = 1101;
	private static final String TAG = "PermissionFragment";
	private static final String PERMISSION_TAG = "TAG_PermissionFragment";

	public static PermissionFragment getInstance(Activity activity) {
		FragmentManager fm = activity.getFragmentManager();
		PermissionFragment fragment = (PermissionFragment)fm.findFragmentByTag(PERMISSION_TAG);
		if (fragment == null) {
			try {
				Log.d(TAG, "Creating PlayGamesFragment");
				fragment = new PermissionFragment();
				FragmentTransaction trans = fm.beginTransaction();
				trans.add(fragment, PERMISSION_TAG);
				trans.commit();
				fm.executePendingTransactions();
			} catch (Throwable th) {
				Log.e(TAG, "Cannot launch PermissionFragment:" + th.getMessage(), th);
				return null;
			}
		}
		return fragment;
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setRetainInstance(true);
	}

	public void acquirePermissions(String permissions[]) {
		FragmentCompat.requestPermissions(this, permissions, PERMISSION_REQUEST_CODE);
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
		
		if (requestCode==PERMISSION_REQUEST_CODE &&
			permissions.length>0) 
		{
			if (grantResults.length>0 &&
					grantResults[0]==PackageManager.PERMISSION_GRANTED)
				Log.d(TAG, "permission granted for " + permissions[0]);
			else
				Log.d(TAG, "permission not granted for " + permissions[0]);
			PermissionHelper.onAcquirePermissions(permissions, grantResults);
		}
	}
}
