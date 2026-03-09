import sys.io.File;
import sys.FileSystem;
import haxe.io.Bytes;
import haxe.io.Path;
import miniaudio.Miniaudio;
import miniaudio.Miniaudio.Buffer;
import miniaudio.Miniaudio.Sound;
import miniaudio.Miniaudio.SoundGroup;

function main()
{
	if (!Miniaudio.init())
	{
		Sys.println('Miniaudio init failed: ' + Miniaudio.describeLastError());
		Sys.exit(1);
	}

	final dir = 'audio';
	if (!FileSystem.exists(dir) || !FileSystem.isDirectory(dir))
	{
		Sys.println('Audio directory not found: ' + dir);
		Miniaudio.uninit();
		Sys.exit(1);
	}

	final files = [
		for (name in FileSystem.readDirectory(dir))
			if (!FileSystem.isDirectory(dir + '/' + name)) dir + '/' + name
	];
	files.sort((a, b) -> a < b ? -1 : a > b ? 1 : 0);

	if (files.length == 0)
	{
		Sys.println('No audio files found in ' + dir);
		Miniaudio.uninit();
		Sys.exit(1);
	}

	var failed = false;
	final group:SoundGroup = new SoundGroup();
	for (path in files)
	{
		final bytes:Bytes = File.getBytes(path);
		final buffer:Buffer = Buffer.fromBytes(bytes);

		if (buffer == null)
		{
			failed = true;
			Sys.println('FAIL ' + path + ': ' + Miniaudio.describeLastError());
			continue;
		}

		final sound:Sound = new Sound(buffer, group);
		if (sound == null)
		{
			#if hlopus
			failed = true;
			Sys.println('FAIL ' + path + ': could not create sound');
			#else
			Sys.println("SKIP " + path + "\nhlopus is not installed!");
			#end

			buffer.dispose();

			continue;
		}

		Sys.println('PLAY ' + path);
		if (!sound.start())
		{
			failed = true;
			Sys.println('FAIL ' + path + ': ' + Miniaudio.describeLastError());
		}
		else
		{
			Sys.sleep(1.5);
			sound.stop();
			Sys.println('OK   ' + path);
		}

		sound.dispose();
		buffer.dispose();
	}

	group.dispose();

	Miniaudio.uninit();

	if (failed)
		Sys.exit(1);

	Sys.println('All audio files loaded successfully.');
}
