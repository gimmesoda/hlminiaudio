import haxe.io.Bytes;
import miniaudio.Miniaudio;
import miniaudio.Miniaudio.Buffer;
import miniaudio.Miniaudio.Sound;
import miniaudio.Miniaudio.SoundGroup;

private function main() {
	if (!Miniaudio.init()) {
		Sys.println("FAIL init: " + Miniaudio.describeLastError());
		Sys.exit(1);
	}
	var failed = false;
	final group = new SoundGroup();

	final issues:Array<{path:String, message:String, kind:String}> = [];
	try {
		for (path in TestSupport.fixtures) {
			final label = TestSupport.fixtureLabel(path);
			try {
				final fixture = TestSupport.decodeFixture(path);
				TestSupport.printPlay(label);
				testBufferLifecycle(fixture.bytes, label, group);
				TestSupport.printOk();
			}
			catch (e) {
				final message = Std.string(e);
				final kind = TestSupport.classifyIssue(path, message);
				issues.push({path: path, message: message, kind: kind});
				if (kind == "unsupported")
					TestSupport.printUnsupported(label, message);
				else {
					failed = true;
					TestSupport.printFail(label, message);
				}
			}
		}
		try {
			testPCMBufferFactories(group);
			TestSupport.printOk("pcm");
		}
		catch (e) {
			failed = true;
			final message = Std.string(e);
			issues.push({path: "pcm", message: message, kind: "fail"});
			TestSupport.printFail("pcm", message);
		}
		try {
			testInvalidInput();
			TestSupport.printOk("invalid");
		}
		catch (e) {
			failed = true;

			final message = Std.string(e);
			issues.push({path: "invalid", message: message, kind: "fail"});
			TestSupport.printFail("invalid", message);
		}
	}
	catch (e) {
		failed = true;
		Sys.println("FAIL " + Std.string(e));
	}
	group.dispose();

	Miniaudio.uninit();
	if (failed) {
		Sys.println("");
		var failedCount = 0;
		for (issue in issues)
			if (issue.kind == "fail")
				failedCount++;
		Sys.println("Failed checks: " + failedCount);
		for (issue in issues)
			if (issue.kind == "fail")
				Sys.println("- " + TestSupport.fixtureLabel(issue.path) + ": " + issue.message);
		Sys.exit(1);
	}
	var unsupportedCount = 0;
	for (issue in issues)
		if (issue.kind == "unsupported")
			unsupportedCount++;
	if (unsupportedCount > 0)
		Sys.println("Unsupported files: " + unsupportedCount);

	Sys.println("HL tests passed.");
}

private function testBufferLifecycle(bytes:Bytes, label:String, group:SoundGroup):Void {
	final buffer = Buffer.fromBytes(bytes);
	TestSupport.assert(buffer != null, label + ": buffer creation failed: " + Miniaudio.describeLastError());
	final sound = new Sound(buffer, group);
	TestSupport.assert(sound != null, label + ": sound creation failed: " + Miniaudio.describeLastError());
	TestSupport.assert(buffer.lengthSamples > 0, label + ": buffer length should be positive");
	TestSupport.assert(buffer.duration > 0, label + ": buffer duration should be positive");
	TestSupport.assertNear(buffer.duration / 1000, buffer.durationSeconds, 0.02, label + ": buffer durationSeconds mismatch");
	TestSupport.assertEquals(buffer.lengthSamples, sound.lengthSamples, label + ": sound length should match buffer");
	TestSupport.assertNear(buffer.duration, sound.duration, 20.0, label + ": sound duration should match buffer");
	TestSupport.assertNear(sound.duration / 1000, sound.durationSeconds, 0.02, label + ": sound durationSeconds mismatch");

	sound.volume = 0.25;
	TestSupport.assertNear(0.25, sound.volume, 1e-6, label + ": volume roundtrip failed");
	sound.pan = -0.5;

	TestSupport.assertNear(-0.5, sound.pan, 1e-6, label + ": pan roundtrip failed");
	sound.pitch = 1.1;
	TestSupport.assertNear(1.1, sound.pitch, 1e-6, label + ": pitch roundtrip failed");
	sound.pitch = 1.0;
	TestSupport.assertNear(1.0, sound.pitch, 1e-6, label + ": pitch reset failed");
	sound.spatializationEnabled = true;
	TestSupport.assert(sound.spatializationEnabled, label + ": spatialization enable failed");

	sound.spatializationEnabled = false;
	sound.pan = 0;
	TestSupport.assertNear(0, sound.pan, 1e-6, label + ": pan reset failed");

	sound.volume = 1.0;
	TestSupport.assertNear(1.0, sound.volume, 1e-6, label + ": volume reset failed");
	TestSupport.assertNear(0, sound.time, 1e-6, label + ": initial time should be zero");
	TestSupport.assertNear(0, sound.timeSeconds, 1e-6, label + ": initial timeSeconds should be zero");
	TestSupport.assertEquals(0, sound.seekSamples(-32), label + ": negative seek should clamp to zero");
	final seekSample = Std.int(sound.lengthSamples * 0.25);
	TestSupport.assertEquals(seekSample, sound.seekSamples(seekSample), label + ": sample seek failed");

	TestSupport.assert(sound.getCursorSamples() >= seekSample, label + ": sample seek cursor mismatch");
	final seekTime = sound.duration * 0.5;
	TestSupport.assertNear(seekTime, sound.seek(seekTime), 20.0, label + ": millisecond seek failed");
	TestSupport.assertNear(seekTime, sound.time, 20.0, label + ": millisecond seek time mismatch");
	final seekTimeSeconds = sound.durationSeconds * 0.25;

	TestSupport.assertNear(seekTimeSeconds, sound.seekSeconds(seekTimeSeconds), 0.02, label + ": second seek failed");
	TestSupport.assertNear(seekTimeSeconds, sound.timeSeconds, 0.03, label + ": second seek time mismatch");
	final seekTimeMs = sound.duration * 0.75;
	TestSupport.assertNear(seekTimeMs, sound.seekMs(seekTimeMs), 20.0, label + ": seekMs alias failed");
	TestSupport.assertNear(seekTimeMs, sound.time, 20.0, label + ": seekMs alias time mismatch");
	sound.time = 0;

	TestSupport.assertNear(0, sound.time, 0.02, label + ": time setter should seek to zero");
	sound.timeSeconds = 0;
	TestSupport.assertNear(0, sound.timeSeconds, 0.02, label + ": timeSeconds setter should seek to zero");
	TestSupport.assert(sound.getCursorSamples() >= 0, label + ": cursor must be non-negative");
	TestSupport.assert(!sound.isPlaying(), label + ": fresh sound should not be playing");
	var completed = 0;

	sound.setOnComplete(() -> completed++);
	TestSupport.assert(sound.start(), label + ": start failed");
	TestSupport.assert(waitUntil(() -> return sound.isPlaying(), 0.25), label + ": sound never entered playing state");
	final startedCursor = sound.getCursorSamples();
	TestSupport.assert(waitUntil(() -> return sound.getCursorSamples() > startedCursor, 0.5), label + ": playback cursor did not advance");

	TestSupport.assert(waitUntil(() -> return !sound.isPlaying(), sound.durationSeconds + 1.0), label + ": sound did not finish playback");
	TestSupport.assert(waitUntil(() -> return completed == 1, 0.25), label + ": completion callback did not fire");
	Miniaudio.update();
	TestSupport.assertEquals(1, completed, label + ": completion callback should fire once");
	sound.clearOnComplete();
	sound.dispose();
	buffer.dispose();
}

private function waitUntil(check:Void->Bool, timeoutSeconds:Float):Bool {
	final deadline = haxe.Timer.stamp() + timeoutSeconds;
	while (haxe.Timer.stamp() < deadline) {
		Miniaudio.update();
		if (check())
			return true;
		Sys.sleep(0.01);
	}
	Miniaudio.update();
	return check();
}

private function testPCMBufferFactories(group:SoundGroup):Void {
	// testing pcm part
	final floatBytes = Bytes.alloc(4 * 2 * 8);
	for (i in 0...8) {
		floatBytes.setFloat(i * 8, 0.25);
		floatBytes.setFloat(i * 8 + 4, -0.25);
	}
	final floatBuffer = Buffer.fromPCMFloat(floatBytes, 2, 48000);
	TestSupport.assert(floatBuffer != null, "fromPCMFloat failed: " + Miniaudio.describeLastError());
	final floatSound = new Sound(floatBuffer, group);
	TestSupport.assert(floatSound != null, "sound from PCM float buffer failed");
	floatSound.dispose();
	floatBuffer.dispose();
	final pcm16Bytes = Bytes.alloc(2 * 2 * 8);
	for (i in 0...8) {
		pcm16Bytes.setUInt16(i * 4, 0x2000);
		pcm16Bytes.setUInt16(i * 4 + 2, 0xE000);
	}
	final pcm16Buffer = Buffer.fromPCM16(pcm16Bytes, 2, 44100);
	TestSupport.assert(pcm16Buffer != null, "fromPCM16 failed: " + Miniaudio.describeLastError());
	final pcm16Sound = new Sound(pcm16Buffer, group);
	TestSupport.assert(pcm16Sound != null, "sound from PCM16 buffer failed");
	pcm16Sound.dispose();
	pcm16Buffer.dispose();
}

private function testInvalidInput():Void {
	final invalid = Bytes.ofString("not audio");
	TestSupport.assert(Miniaudio.decodeToPCMFloat(invalid) == null, "invalid float decode should fail");
	TestSupport.assert(Miniaudio.describeLastError().length > 0, "invalid float decode should populate error");
	TestSupport.assert(Miniaudio.decodeToPCM16(invalid) == null, "invalid s16 decode should fail");
	TestSupport.assert(Miniaudio.describeLastError().length > 0, "invalid s16 decode should populate error");
	TestSupport.assert(Buffer.fromBytes(invalid) == null, "invalid buffer decode should fail");
	TestSupport.assert(Miniaudio.describeLastError().length > 0, "invalid buffer decode should populate error");
	TestSupport.assert(Buffer.fromPCMFloat(Bytes.alloc(3), 2, 44100) == null, "misaligned float PCM should fail");
	TestSupport.assert(Buffer.fromPCM16(Bytes.alloc(3), 2, 44100) == null, "misaligned s16 PCM should fail");
}
