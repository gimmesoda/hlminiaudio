package hxd.snd.miniaudio;

import miniaudio.Miniaudio.Buffer as MiniBuffer;
import miniaudio.Miniaudio.Sound as MiniSound;

class BufferHandle
{
	public var inst:MiniBuffer;
	public var isEnd:Bool;
	public var samples:Int;
	public var sampleRate:Int;

	public function new() {}
}

class SourceHandle
{
	public var inst:MiniSound;
	public var sampleOffset:Int;
	public var queuedStart:Int;
	public var playing:Bool;
	public var volume:Float;
	public var processed:Bool;

	var nextAuxiliarySend:Int;
	var freeAuxiliarySends:Array<Int>;
	var effectToAuxiliarySend:Map<hxd.snd.Effect, Int>;

	public function new()
	{
		sampleOffset = 0;
		playing = false;
		queuedStart = 0;
		volume = 1.0;
		processed = false;
		nextAuxiliarySend = 0;
		freeAuxiliarySends = [];
		effectToAuxiliarySend = new Map();
	}

	public function acquireAuxiliarySend(effect:hxd.snd.Effect):Int
	{
		var send = freeAuxiliarySends.length > 0 ? freeAuxiliarySends.shift() : nextAuxiliarySend++;
		effectToAuxiliarySend.set(effect, send);
		return send;
	}

	public function getAuxiliarySend(effect:hxd.snd.Effect):Int
	{
		return effectToAuxiliarySend.get(effect);
	}

	public function releaseAuxiliarySend(effect:hxd.snd.Effect):Int
	{
		var send = effectToAuxiliarySend.get(effect);
		effectToAuxiliarySend.remove(effect);
		freeAuxiliarySends.push(send);
		return send;
	}
}
