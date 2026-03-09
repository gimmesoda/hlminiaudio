package miniaudio.heaps;

import haxe.io.Path;
import hxd.fs.FileEntry;

class AudioBuffer extends hxd.res.Sound
{
	var cachedData:Null<hxd.snd.Data>;

	public function new(entry:FileEntry)
	{
		super(entry);
		entry.watch(() -> cachedData = null);
	}

	override public function getData():hxd.snd.Data
	{
		if (cachedData != null)
			return cachedData;

		final bytes = entry.getBytes();
		final ext = Path.extension(entry.path).toLowerCase();

		cachedData = switch (ext)
		{
			case 'ogg', 'opus', 'flac', 'wav', 'mp3', 'aiff':
				loadWithMiniaudio(bytes);
			default:
				super.getData();
		};

		if (cachedData == null)
			throw '${entry.path}: ${miniaudio.Miniaudio.describeLastError()}';

		return cachedData;
	}

	override public function dispose()
	{
		stop();
		cachedData = null;
	}

	static function loadWithMiniaudio(bytes:haxe.io.Bytes):hxd.snd.Data
	{
		final decoded = miniaudio.Miniaudio.decodeToPCMFloat(bytes);
		if (decoded == null)
			return null;

		return new AudioData(decoded.bytes, decoded.channels, decoded.sampleRate, decoded.samples, decoded.floatFormat);
	}
}
