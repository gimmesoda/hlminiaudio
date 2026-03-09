import h2d.Text;
import haxe.Timer;
import haxe.io.Path;
import miniaudio.Miniaudio;
import miniaudio.Miniaudio.Buffer;
import miniaudio.Miniaudio.Sound;
import miniaudio.Miniaudio.SoundGroup;
import sys.FileSystem;

class TestHeaps extends hxd.App
{
	static function main()
	{
		new TestHeaps();
	}

	final audioDir = "audio";
	final playTime = 1.5;

	var files:Array<String> = [];
	var index = 0;
	var currentBuffer:Buffer;
	var currentSound:Sound;
	var group:SoundGroup;
	var playStartedAt = 0.0;
	var label:Text;
	var failed = false;

	override function init()
	{
		label = new Text(hxd.res.DefaultFont.get(), s2d);
		label.x = 20;
		label.y = 20;

		if (!Miniaudio.init())
		{
			showAndExit("Miniaudio init failed: " + Miniaudio.describeLastError());
			return;
		}

		if (!FileSystem.exists(audioDir) || !FileSystem.isDirectory(audioDir))
		{
			showAndExit("Audio directory not found: " + audioDir);
			return;
		}

		files = [
			for (name in FileSystem.readDirectory(audioDir))
				if (!FileSystem.isDirectory(audioDir + "/" + name)) audioDir + "/" + name
		];
		files.sort((a, b) -> a < b ? -1 : a > b ? 1 : 0);

		if (files.length == 0)
		{
			showAndExit("No audio files found in " + audioDir);
			return;
		}

		group = new SoundGroup();
		playNext();
	}

	override function update(dt:Float)
	{
		super.update(dt);

		// timer
		if (currentSound != null && Timer.stamp() - playStartedAt >= playTime)
		{
			currentSound.stop();
			disposeCurrent();
			index++;
			playNext();
		}

		if (hxd.Key.isPressed(hxd.Key.ESCAPE))
			finish();
	}

	function playNext()
	{
		if (index >= files.length)
		{
			showAndExit(failed ? "Playback test finished with errors." : "Playback test finished successfully.");
			return;
		}

		final path = files[index];
		label.text = "Loading " + path;

		final resource = hxd.res.Any.fromBytes(path, sys.io.File.getBytes(path));
		currentBuffer = resource.to(miniaudio.heaps.AudioBuffer).load();
		if (currentBuffer == null)
		{
			#if hlopus
			failed = true;
			label.text = "FAIL " + path + "\n" + Miniaudio.describeLastError();
			#else
			label.text = "SKIP " + path + "\nOpus is not supported in this build";
			#end

			Sys.println(label.text);
			index++;
			playNext();
			return;
		}

		currentSound = new Sound(currentBuffer, group);
		if (currentSound == null)
		{
			failed = true;
			label.text = "FAIL " + path + "\nCould not create sound";
			Sys.println(label.text);
			disposeCurrent();
			index++;
			playNext();
			return;
		}

		if (!currentSound.start())
		{
			failed = true;
			label.text = "FAIL " + path + "\n" + Miniaudio.describeLastError();
			Sys.println(label.text);
			disposeCurrent();
			index++;
			playNext();
			return;
		}

		playStartedAt = Timer.stamp();
		label.text = "Playing " + path + "\nPress ESC to exit";
		Sys.println("PLAY " + path);
	}

	function disposeCurrent()
	{
		if (currentSound != null)
		{
			currentSound.dispose();
			currentSound = null;
		}

		if (currentBuffer != null)
		{
			currentBuffer.dispose();
			currentBuffer = null;
		}
	}

	function showAndExit(message:String)
	{
		label.text = message;
		Sys.println(message);
		finish();
	}

	function finish()
	{
		disposeCurrent();

		if (group != null)
		{
			group.dispose();
			group = null;
		}

		Miniaudio.uninit();
		hxd.System.exit();
	}
}
