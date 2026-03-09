import haxe.io.Bytes;
import haxe.io.Path;
import hxd.Res;
import hxd.fs.LocalFileSystem;
import hxd.res.Loader;
import hxd.snd.Data.SampleFormat;
import miniaudio.Miniaudio;
import miniaudio.heaps.AudioBuffer;

class TestHeaps
{
	static function main()
	{
		Res.loader = new Loader(new LocalFileSystem('audio', null));

		if (!Miniaudio.init())
		{
			Sys.println('FAIL init: ' + Miniaudio.describeLastError());
			Sys.exit(1);
		}

		var failed = false;

		try
		{
			for (path in TestSupport.fixtures)
			{
				testResource(path);
				TestSupport.printPass(TestSupport.fixtureLabel(path));
			}
		}
		catch (e)
		{
			failed = true;
			Sys.println('FAIL ' + Std.string(e));
		}

		Miniaudio.uninit();

		if (failed)
			Sys.exit(1);

		Sys.println('All deterministic Heaps tests passed.');
	}

	static function testResource(path:String):Void
	{
		final resourcePath = Path.withoutDirectory(path);
		final resource:AudioBuffer = cast Res.load(resourcePath);
		TestSupport.assert(resource != null, TestSupport.fixtureLabel(path) + ': resource load failed');

		final direct = TestSupport.decodeFixture(path);
		final dataA = resource.getData();
		final dataB = resource.getData();
		TestSupport.assert(dataA == dataB, TestSupport.fixtureLabel(path) + ': resource data cache should be reused');

		TestSupport.assertEquals(direct.floatDecoded.channels, dataA.channels, TestSupport.fixtureLabel(path) + ': channel count mismatch');
		TestSupport.assertEquals(direct.floatDecoded.sampleRate, dataA.samplingRate, TestSupport.fixtureLabel(path) + ': sample rate mismatch');
		TestSupport.assertEquals(direct.floatDecoded.samples, dataA.samples, TestSupport.fixtureLabel(path) + ': sample count mismatch');
		TestSupport.assertEquals(SampleFormat.F32, dataA.sampleFormat, TestSupport.fixtureLabel(path) + ': heaps data should be float');

		final samplesToCheck = direct.floatDecoded.samples < 8 ? direct.floatDecoded.samples : 8;
		final decodedSlice = Bytes.alloc(samplesToCheck * dataA.getBytesPerSample());
		dataA.decode(decodedSlice, 0, 0, samplesToCheck);
		TestSupport.assertBytesEqual(direct.floatDecoded.bytes.sub(0, decodedSlice.length), decodedSlice, TestSupport.fixtureLabel(path) + ': decoded audio mismatch');

		resource.dispose();
		final dataC = resource.getData();
		TestSupport.assert(dataC != dataA, TestSupport.fixtureLabel(path) + ': dispose should clear cached data');
	}
}
