using System.IO;

using Cogl;
using NUnit.Framework;

namespace Cogl.Test
{
    [TestFixture]
    public class TestTexture : AssertionHelper
    {
        private Context ctx;

        [SetUp]
        public void Init()
        {
           ctx = new Context();
        }

        [Test]
        public void NewFromFile()
        {
            Texture tex;

            tex = new Texture(Path.Combine(Config.TESTSDIR, "smiley.png"));

            Expect(tex.GetWidth(), EqualTo(100));
            Expect(tex.GetHeight(), EqualTo(100));
        }
    }
}
