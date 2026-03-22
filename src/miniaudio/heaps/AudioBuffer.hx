package miniaudio.heaps;

import haxe.io.Path;

class AudioBuffer extends hxd.res.Sound
{
	private var buffer:Null<Miniaudio.Buffer>;

	public function getBuffer():Miniaudio.Buffer
	{
		buffer ??= Miniaudio.Buffer.fromBytes(entry.getBytes());
		if (buffer == null)
			throw '${entry.path}: ${Miniaudio.describeLastError()}';

		return buffer;
	}
}
