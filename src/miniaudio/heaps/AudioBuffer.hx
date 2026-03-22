package miniaudio.heaps;

import hxd.fs.FileEntry;
import hxd.res.Resource;

class AudioBuffer extends Resource
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
